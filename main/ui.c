#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "ui.h"
#include "led_driver.h"
#include "view.h"
#include "animation.h"

static const char *TAG = "WEATHER_STATION: UI";

uint8_t Btn_array[4] = {BTN_1, BTN_2, BTN_3, BTN_4};
uint8_t Btn_state;  // 4th bit=BTN_4, LSB=BTN_1

uint8_t Enc_array[4] = {ENC_1_A, ENC_1_B, ENC_2_A, ENC_2_B};
// Previous encoder pins smushed into one byte:
// xxxx 2_B 2_A 1_B 1_A
static volatile uint8_t Prev_encoder_states;

// Button interrupt handling
TaskHandle_t periodicTaskHandle_btn = NULL;
void button_event_task(void *);

// encoder interrupt handling
TaskHandle_t periodicTaskHandle_enc = NULL; // now used for encoder event consumer
void encoder_event_task(void *);

// ISR queues for button and encoder pin changes
static QueueHandle_t button_isr_queue = NULL;
static QueueHandle_t encoder_isr_queue = NULL;

// ISR handlers
static void IRAM_ATTR button_isr_handler(void *arg);
static void IRAM_ATTR encoder_isr_handler(void *arg);

// last observed pin level (indexed by Enc_array index). Updated in ISR to avoid repeated queuing
static volatile uint8_t encoder_last_level[4] = {0,0,0,0};

// last observed button pin levels (indexed by button index)
static volatile uint8_t button_last_level[4] = {0,0,0,0};

// Button pressed state (shared between button and encoder tasks for paint mode)
static volatile uint8_t button_pressed_state[4] = {0,0,0,0};

// Private prototypes
void setup_gpio_input(uint8_t);

// Public API to check button state (for paint mode, etc.)
uint8_t Ui__Is_Button_Pressed(uint8_t button_num);

// PUBLIC FUNCTIONS
void Ui__Initialize(void) {
    Btn_state = 0;
    
    for(uint8_t btn=0; btn < 4; btn++) {
        uint8_t btn_num = Btn_array[btn];
        setup_gpio_input(btn_num);
    }

    Prev_encoder_states = 0;    // xxxx 2_B 2_A 1_B 1_A     ex: 0000 1011 = 2_B=hi, 2_A=low, ...
    for(uint8_t enc=0; enc < 4; enc++) {
        uint8_t enc_pin_num = Enc_array[enc];
        setup_gpio_input(enc_pin_num);
        Prev_encoder_states |= (gpio_get_level(enc_pin_num) << enc);
        // initialize last-level cache used by ISR-level filtering
        encoder_last_level[enc] = (uint8_t)gpio_get_level(enc_pin_num);
    }

    // Create queues for button and encoder ISR events
    button_isr_queue = xQueueCreate(16, sizeof(uint32_t));
    encoder_isr_queue = xQueueCreate(16, sizeof(uint32_t));

    // Install GPIO ISR service (once for all GPIO interrupts)
    gpio_install_isr_service(0);
    
    // Attach button interrupt handlers
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pin = Btn_array[i];
        button_last_level[i] = (uint8_t)gpio_get_level(pin);
        gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(pin, button_isr_handler, (void*)(uintptr_t)pin);
    }
    
    // Attach encoder interrupt handlers
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pin = Enc_array[i];
        gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(pin, encoder_isr_handler, (void*)(uintptr_t)pin);
    }

    // start consumer tasks that process ISR events and debounce
    xTaskCreate(
        button_event_task,              // Task function
        "ButtonEventTask",             // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        (void*)button_isr_queue,        // Task parameter -> queue handle
        10,                             // Task priority
        &periodicTaskHandle_btn         // Task handle
    );
    xTaskCreate(
        encoder_event_task,             // Task function
        "EncoderEventTask",            // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        (void*)encoder_isr_queue,       // Task parameter -> queue handle
        10,                             // Task priority
        &periodicTaskHandle_enc         // Task handle
    );

}

// Private methods

// Button ISR: push pin number to queue when edge detected
static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (!button_isr_queue) return;

    // find index for this pin (0-3)
    int idx = -1;
    for (int i = 0; i < 4; ++i) {
        if (Btn_array[i] == (uint8_t)pin) { idx = i; break; }
    }
    if (idx < 0) return;

    // read level; only enqueue if different from last seen level for this pin
    uint8_t level = (uint8_t)gpio_get_level((gpio_num_t)pin);
    if (level == button_last_level[idx]) {
        return; // duplicate edge or noise already processed
    }

    // update last level (done in ISR context)
    button_last_level[idx] = level;

    xQueueSendFromISR(button_isr_queue, &pin, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// Consumer task: debounce and post button down/up events
void button_event_task(void *pvParameters) {
    QueueHandle_t q = (QueueHandle_t)pvParameters;
    const TickType_t debounce_ticks = pdMS_TO_TICKS(UI_BUTTON_DEBOUNCE_MS);
    const TickType_t refractory_ticks = pdMS_TO_TICKS(UI_BUTTON_REFRACTORY_MS);
    
    // Track button state
    uint8_t pressed[4] = {0, 0, 0, 0};
    TickType_t next_allowed[4] = {0, 0, 0, 0};

    while (1) {
        uint32_t pin;
        if (xQueueReceive(q, &pin, pdMS_TO_TICKS(10)) == pdTRUE) {
            // find which button
            int idx = -1;
            for (int i = 0; i < 4; i++) {
                if (Btn_array[i] == (uint8_t)pin) { idx = i; break; }
            }
            if (idx < 0) continue;

            TickType_t now = xTaskGetTickCount();
            if (now < next_allowed[idx]) {
                continue; // still in refractory window
            }

            // Debounce: sample, wait, resample
            uint8_t sample1 = gpio_get_level((gpio_num_t)pin);
            vTaskDelay(debounce_ticks);
            uint8_t sample2 = gpio_get_level((gpio_num_t)pin);
            
            if (sample1 != sample2) {
                // unstable, skip this transient
                next_allowed[idx] = xTaskGetTickCount() + pdMS_TO_TICKS(5);
                continue;
            }

            uint8_t is_pressed = (sample2 == PRESSED);

            if (is_pressed && !pressed[idx]) {
                // Button just pressed
                pressed[idx] = 1;
                button_pressed_state[idx] = 1;  // Update shared state for encoder task
                ESP_LOGI(TAG, "Button %d DOWN", idx + 1);
                EventSystem_PostEvent(EVENT_UI_BUTTON_DOWN, (1 << idx), NULL);
            } 
            else if (!is_pressed && pressed[idx]) {
                // Button just released
                pressed[idx] = 0;
                button_pressed_state[idx] = 0;  // Update shared state for encoder task
                ESP_LOGI(TAG, "Button %d UP", idx + 1);
                EventSystem_PostEvent(EVENT_UI_BUTTON_UP, (1 << idx), NULL);
                
                // Set refractory period to avoid bounce
                next_allowed[idx] = xTaskGetTickCount() + refractory_ticks;
            }
        }
    }
}

// ISR: push pin number to queue
static void IRAM_ATTR encoder_isr_handler(void *arg) {
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (!encoder_isr_queue) return;

    // find index for this pin (0-3)
    int idx = -1;
    for (int i = 0; i < 4; ++i) {
        if (Enc_array[i] == (uint8_t)pin) { idx = i; break; }
    }
    if (idx < 0) return;

    // read level; only enqueue if different from last seen level for this pin
    uint8_t level = (uint8_t)gpio_get_level((gpio_num_t)pin);
    if (level == encoder_last_level[idx]) {
        return; // duplicate edge or noise already processed
    }

    // update last level (done in ISR context)
    encoder_last_level[idx] = level;

    xQueueSendFromISR(encoder_isr_queue, &pin, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// Consumer task: debounce and translate pin changes into encoder rotation events
void encoder_event_task(void *pvParameters) {
    QueueHandle_t q = (QueueHandle_t)pvParameters;
    const TickType_t debounce_ticks = pdMS_TO_TICKS(8);    // small sample debounce
    const TickType_t refractory_ticks = pdMS_TO_TICKS(30); // suppress further events for this encoder
    TickType_t next_allowed[2] = {0,0};

    while (1) {
        uint32_t pin;
        if (xQueueReceive(q, &pin, portMAX_DELAY) == pdTRUE) {
            // find which encoder and which bit
            int idx = -1;
            for (int i = 0; i < 4; i++) {
                if (Enc_array[i] == (uint8_t)pin) { idx = i; break; }
            }
            if (idx < 0) continue;

            int encoder = idx / 2; // 0 or 1
            TickType_t now = xTaskGetTickCount();
            if (now < next_allowed[encoder]) {
                continue; // still in refractory window
            }

            // small debounce: sample, wait, resample
            uint8_t a1 = gpio_get_level(Enc_array[encoder*2]);
            uint8_t b1 = gpio_get_level(Enc_array[encoder*2 + 1]);
            vTaskDelay(debounce_ticks);
            uint8_t a2 = gpio_get_level(Enc_array[encoder*2]);
            uint8_t b2 = gpio_get_level(Enc_array[encoder*2 + 1]);
            if (a1 != a2 || b1 != b2) {
                // unstable, skip this transient
                // set a small backoff to avoid tight requeueing
                next_allowed[encoder] = xTaskGetTickCount() + pdMS_TO_TICKS(5);
                continue;
            }

            uint8_t encbits = (a2 ? 1 : 0) | ((b2 ? 1 : 0) << 1);
            uint8_t prev = (Prev_encoder_states >> (encoder*2)) & 3;

            uint8_t enc_events = 0;
            if (encbits != prev) {
                if (encbits == 3) { // detent
                    if (prev == 1) {
                        enc_events = (encoder == 0) ? 0x10 : 0x40; // CW
                    } else if (prev == 2) {
                        enc_events = (encoder == 0) ? 0x20 : 0x80; // CCW
                    }
                }
                // update Prev_encoder_states for this encoder
                Prev_encoder_states &= ~(3 << (encoder*2));
                Prev_encoder_states |= (encbits << (encoder*2));
            }

            if (enc_events) {
                // ESP_LOGI(TAG, "enc events (ISR): encoder=%d bits=0x%02x prev=0x%02x events=0x%02x",
                //          encoder, encbits, prev, enc_events);
                
                // Check if any button is currently held (for paint mode in etchsketch)
                // The view module can check button_pressed_state[] to enable paint trail
                uint8_t any_button_held = 0;
                for (int i = 0; i < 4; i++) {
                    if (button_pressed_state[i]) {
                        any_button_held = 1;
                        // ESP_LOGI(TAG, "Encoder rotated while Button %d held (paint mode)", i + 1);
                        break;
                    }
                }
                
                // post event; if the event system queue is full we will drop the event to avoid blocking ISR
                EventSystem_PostEvent(EVENT_UI_ENCODER, enc_events, NULL);
                // suppress further events for a short refractory period to avoid floods
                next_allowed[encoder] = xTaskGetTickCount() + refractory_ticks;
            } else {
                // no event, allow next sample shortly
                next_allowed[encoder] = xTaskGetTickCount() + pdMS_TO_TICKS(3);
            }
        }
    }
}

void setup_gpio_input(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    // enable internal pull-up to avoid floating inputs; adjust to PULLDOWN if your hardware is active-high
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
}

// Check if a specific button is currently pressed (button_num: 1-4)
uint8_t Ui__Is_Button_Pressed(uint8_t button_num) {
    if (button_num < 1 || button_num > 4) return 0;
    return button_pressed_state[button_num - 1];
}
