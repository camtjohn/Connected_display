#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
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

// Button interrupt handling
TaskHandle_t periodicTaskHandle_btn = NULL;
void button_event_task(void *);

// ISR queue for button pin changes
static QueueHandle_t button_isr_queue = NULL;

// Button ISR handler
static void button_isr_handler(void *arg);

// last observed button pin levels (indexed by button index)
static volatile uint8_t button_last_level[4] = {0,0,0,0};

// Built-in button (GPIO 0) long-press detection using interrupts
static volatile uint32_t builtin_btn_press_time_ms = 0;
static volatile uint8_t builtin_btn_longpress_detected = 0;
static esp_timer_handle_t builtin_btn_timer_handle = NULL;

static void builtin_button_isr_handler(void *arg);
static void builtin_button_timer_callback(void *arg);

// PCNT encoder units
static pcnt_unit_handle_t pcnt_unit_enc1 = NULL;
static pcnt_unit_handle_t pcnt_unit_enc2 = NULL;
static void encoder_poll_task(void *arg);

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

    // Configure encoder GPIO pins as inputs with pullups
    for(uint8_t enc=0; enc < 4; enc++) {
        uint8_t enc_pin_num = Enc_array[enc];
        setup_gpio_input(enc_pin_num);
    }
    
    // Create queue for button ISR events
    button_isr_queue = xQueueCreate(16, sizeof(uint32_t));

    // === Minimal PCNT Setup for Encoder 1 (GPIO 2=A, GPIO 1=B) ===
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit_enc1));
    
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,  // 1us filter
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_enc1, &filter_config));
    
    // Channel A
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENC_1_A,
        .level_gpio_num = ENC_1_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_enc1, &chan_a_config, &pcnt_chan_a));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
                                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
                                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    
    // Channel B
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENC_1_B,
        .level_gpio_num = ENC_1_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_enc1, &chan_b_config, &pcnt_chan_b));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
                                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
                                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_enc1));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_enc1));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_enc1));
    
    // === Encoder 2 (GPIO 3=A, GPIO 4=B) ===
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit_enc2));
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_enc2, &filter_config));
    
    pcnt_chan_config_t chan2_a_config = {
        .edge_gpio_num = ENC_2_A,
        .level_gpio_num = ENC_2_B,
    };
    pcnt_channel_handle_t pcnt_chan2_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_enc2, &chan2_a_config, &pcnt_chan2_a));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan2_a,
                                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan2_a,
                                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    
    pcnt_chan_config_t chan2_b_config = {
        .edge_gpio_num = ENC_2_B,
        .level_gpio_num = ENC_2_A,
    };
    pcnt_channel_handle_t pcnt_chan2_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_enc2, &chan2_b_config, &pcnt_chan2_b));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan2_b,
                                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan2_b,
                                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_enc2));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_enc2));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_enc2));

    // Install GPIO ISR service (once for all GPIO interrupts)
    gpio_install_isr_service(0);
    
    // Attach button interrupt handlers
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pin = Btn_array[i];
        button_last_level[i] = (uint8_t)gpio_get_level(pin);
        gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(pin, button_isr_handler, (void*)(uintptr_t)pin);
    }
    
    // start consumer tasks
    xTaskCreate(
        button_event_task,              // Task function
        "ButtonEventTask",             // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        (void*)button_isr_queue,        // Task parameter -> queue handle
        10,                             // Task priority
        &periodicTaskHandle_btn         // Task handle
    );
    
    xTaskCreate(
        encoder_poll_task,              // Task function
        "EncoderPollTask",             // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        10,                             // Task priority
        NULL                            // Task handle
    );
    
    // Setup built-in button (GPIO 0) for long-press detection via interrupt
    gpio_config_t builtin_btn_config = {
        .pin_bit_mask = (1ULL << BTN_BUILTIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,  // Trigger on both press and release
    };
    ESP_ERROR_CHECK(gpio_config(&builtin_btn_config));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_BUILTIN, builtin_button_isr_handler, NULL));
    
    // Create high-resolution timer for long-press detection
    const esp_timer_create_args_t timer_args = {
        .callback = builtin_button_timer_callback,
        .name = "builtin_btn_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &builtin_btn_timer_handle));
}
}

// Poll PCNT counters and post events when movement detected
static void encoder_poll_task(void *arg) {
    int prev_count1 = 0, prev_count2 = 0;
    
    while (1) {
        int count1 = 0, count2 = 0;
        pcnt_unit_get_count(pcnt_unit_enc1, &count1);
        pcnt_unit_get_count(pcnt_unit_enc2, &count2);
        
        // Encoder 1: trigger on every 4 counts (one detent)
        int delta1 = count1 - prev_count1;
        if (delta1 >= 4) {
            EventSystem_PostEvent(EVENT_UI_ENCODER, 0x10, NULL);  // CCW
            prev_count1 = count1;
        } else if (delta1 <= -4) {
            EventSystem_PostEvent(EVENT_UI_ENCODER, 0x20, NULL);  // CW
            prev_count1 = count1;
        }
        
        // Encoder 2
        int delta2 = count2 - prev_count2;
        if (delta2 >= 4) {
            EventSystem_PostEvent(EVENT_UI_ENCODER, 0x40, NULL);  // CCW
            prev_count2 = count2;
        } else if (delta2 <= -4) {
            EventSystem_PostEvent(EVENT_UI_ENCODER, 0x80, NULL);  // CW
            prev_count2 = count2;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Poll every 10ms
    }
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

// ISR handler for built-in button (GPIO 0)
static void builtin_button_isr_handler(void *arg) {
    uint8_t btn_level = gpio_get_level(BTN_BUILTIN);
    
    if (btn_level == 0) {  // Button pressed (active LOW)
        // Start timer to detect long press
        esp_timer_start_once(builtin_btn_timer_handle, BTN_LONGPRESS_MS * 1000);  // Convert ms to µs
    } else {  // Button released
        // Stop timer
        esp_timer_stop(builtin_btn_timer_handle);
    }
}

// Timer callback - fires if button held for 2+ seconds
static void builtin_button_timer_callback(void *arg) {
    // Only trigger if button is still pressed
    if (gpio_get_level(BTN_BUILTIN) == 0) {
        ESP_LOGI(TAG, "Built-in button LONG PRESS - rebooting to factory app");
        OTA__Request_Factory_Boot();  // Reboot to factory app (does not return)
    }
}}
