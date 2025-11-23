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

// Task variables
TaskHandle_t periodicTaskHandle_btn = NULL;
void periodic_task_poll_buttons(void *);
// encoder interrupt handling
TaskHandle_t periodicTaskHandle_enc = NULL; // now used for encoder event consumer
void encoder_event_task(void *);

// ISR queue for encoder pin changes
static QueueHandle_t encoder_isr_queue = NULL;

// ISR handler prototype
static void IRAM_ATTR encoder_isr_handler(void *arg);

// last observed pin level (indexed by Enc_array index). Updated in ISR to avoid repeated queuing
static volatile uint8_t encoder_last_level[4] = {0,0,0,0};

// Private prototypes
uint8_t poll_btns(void);
// uint8_t poll_encoders(void);
void setup_gpio_input(uint8_t);

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

    xTaskCreate(
        periodic_task_poll_buttons,     // Task function
        "PeriodicTask_PollButtons",     // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        10,                             // Task priority
        &periodicTaskHandle_btn         // Task handle
    );

    // Create queue for encoder ISR events
    encoder_isr_queue = xQueueCreate(16, sizeof(uint32_t));

    // Install GPIO ISR service and attach handlers for encoder pins
    gpio_install_isr_service(0);
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t pin = Enc_array[i];
        gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
        // attach common ISR, pass pin number as arg
        gpio_isr_handler_add(pin, encoder_isr_handler, (void*)(uintptr_t)pin);
    }

    // start consumer task that processes encoder ISR events and debounces
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

// check all buttons. return byte with bit set for buttons that are pressed
// read/modify Btn_state to track prev state
// Return: button number 1-4
uint8_t poll_btns(void) {
    uint8_t btn_events = 0;

    // If press btn, latch and do something. Only unlatch if detect no press.
    for(uint8_t btn=0; btn < 4; btn++) {
        uint8_t btn_num = Btn_array[btn];
        uint8_t prev_state = (Btn_state >> btn) & 1;
        uint8_t curr_state = gpio_get_level(btn_num);

        if(prev_state == NOT_PRESSED) {
            if(curr_state == PRESSED) {
                Btn_state |= (1 << btn);
                btn_events |= (1 << btn);
            }
        } else {        // previously btn pressed
            if(curr_state == NOT_PRESSED) {
                Btn_state &= ~(1 << btn);   // reset state to unpressed
            }
        }
    }

    return (btn_events);
}

// // read encoders. return direction of rotation if there is one.
// uint8_t poll_encoders(void) {
//     uint8_t enc_events = 0;
//     uint8_t curr_encoder_states = 0;

//     // read current encoder pins into bits
//     for(uint8_t enc=0; enc < 4; enc++) {
//         curr_encoder_states |= (gpio_get_level(Enc_array[enc]) << enc);
//     }

//     if (curr_encoder_states != Prev_encoder_states) {   // an encoder moved, not necessarily full event
//         // Simple debounce: re-read after small delay and require stability
//         vTaskDelay(pdMS_TO_TICKS(2));
//         uint8_t curr_encoder_states2 = 0;
//         for(uint8_t enc=0; enc < 4; enc++) {
//             curr_encoder_states2 |= (gpio_get_level(Enc_array[enc]) << enc);
//         }

//         if (curr_encoder_states2 != curr_encoder_states) {
//             // noisy transition; update snapshot and skip
//             Prev_encoder_states = curr_encoder_states2;
//             return 0;
//         }

//         // enc 1 (bits 0-1)
//         if ((curr_encoder_states & 3) == 3) {   // current state at detent
//             if ((Prev_encoder_states & 3) == 1) {
//                 enc_events |= 0x10;     // enc1 CW
//             } else if ((Prev_encoder_states & 3) == 2) {
//                 enc_events |= 0x20;     // enc1 CCW
//             }
//         }
//         // enc 2 (bits 2-3)
//         if (((curr_encoder_states >> 2) & 3) == 3) {   // current state at detent
//             if (((Prev_encoder_states >> 2) & 3) == 1) {
//                 enc_events |= 0x40;     // enc2 CW
//             } else if (((Prev_encoder_states >> 2) & 3) == 2) {
//                 enc_events |= 0x80;     // enc2 CCW
//             }
//         }
//     }

//     if (enc_events) {
//         // ESP_LOGI(TAG, "enc events: curr=0x%02x prev=0x%02x events=0x%02x",
//                 //  curr_encoder_states, Prev_encoder_states, enc_events);
//     }

//     Prev_encoder_states = curr_encoder_states;

//     return enc_events;
// }


// // New event-driven functions that return state without directly processing
// uint8_t btns_interrupt_get_state(void) {
//     uint8_t btn_events = 0;

//     // If press btn, latch and do something. Only unlatch if detect no press.
//     for(uint8_t btn=0; btn < 4; btn++) {
//         uint8_t btn_num = Btn_array[btn];
//         uint8_t prev_state = (Btn_state >> btn) & 1;
//         uint8_t curr_state = gpio_get_level(btn_num);

//         if(prev_state == NOT_PRESSED) {
//             if(curr_state == PRESSED) {
//                 Btn_state |= (1 << btn);
//                 btn_events |= (1 << btn);
//             }
//         } else {        // previously btn pressed
//             if(curr_state == NOT_PRESSED) {
//                 Btn_state &= ~(1 << btn);   // reset state to unpressed
//             }
//         }
//     }

//     return btn_events;
// }

// uint8_t encoders_interrupt_get_state(void) {
//     uint8_t enc_events = 0;
//     uint8_t curr_encoder_states = 0;
//     for(uint8_t enc=0; enc < 4; enc++) {
//         curr_encoder_states |= (gpio_get_level(Enc_array[enc]) << enc);
//     }

//     if (curr_encoder_states != Prev_encoder_states) {   // an encoder moved, not necessarily full event
//         // enc 1
//         if ((curr_encoder_states & 3) == 3) {   // current state at detent
//             if ((Prev_encoder_states & 3) == 1) {
//                 enc_events |= 0x10;     // enc1 CW
//             } else if ((Prev_encoder_states & 3) == 2) {
//                 enc_events |= 0x20;     // enc1 CCW
//             }
//         }
//         // enc 2
//         if ((curr_encoder_states >> 2) == 3) {   // current state at detent
//             if ((Prev_encoder_states >> 2) == 1) {
//                 enc_events |= 0x40;     // enc2 CW
//             } else if ((Prev_encoder_states >> 2) == 2) {
//                 enc_events |= 0x80;     // enc2 CCW
//             }
//         }
//     }
//     Prev_encoder_states = curr_encoder_states;
//     return enc_events;
// }


// // Define task: Periodically poll encoders (now posts events instead of direct processing)
// void periodic_task_poll_encoders(void *pvParameters) {
//     // After method runs, delay for this amount of ms
//     const TickType_t run_every_ms = pdMS_TO_TICKS(UI_ENCODER_TASK_PERIOD_MS);
    
//     while (1) {
//         TickType_t time_start_task = xTaskGetTickCount();
        
//         // Poll encoders and post events for any rotation
//         // encoder events:  enc1 rotate CW: set bit0, rotate CCW: set bit1
//         //                  enc2 rotate CW: set bit2, rotate CCW: set bit3
//         uint8_t encoder_events = poll_encoders();
        
//         if (encoder_events) {
//             EventSystem_PostEvent(EVENT_UI_ENCODER, encoder_events, NULL);  // Encoder 0
//         }
//         vTaskDelayUntil(&time_start_task, run_every_ms);
//     }
// }

// Define task: Periodically poll buttons (now posts events instead of direct processing)
void periodic_task_poll_buttons(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_BUTTON_TASK_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        
        // Poll buttons and post events for any pressed buttons
        // Note: This would ideally be replaced with interrupt-driven GPIO handling
        uint8_t button_states = poll_btns();
        
        if (button_states) {
            EventSystem_PostEvent(EVENT_UI_BUTTON_PRESS, button_states, NULL);
        }
        
        vTaskDelayUntil(&time_start_task, run_every_ms);
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
