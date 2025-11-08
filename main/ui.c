#include "esp_log.h"
#include "driver/gpio.h"

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
uint8_t Prev_encoder_states;

// Task variables
TaskHandle_t periodicTaskHandle_btn = NULL;
void periodic_task_poll_buttons(void *);
TaskHandle_t periodicTaskHandle_enc = NULL;
void periodic_task_poll_encoders(void *);

// Private prototypes
uint8_t poll_btns(void);
uint8_t poll_encoders(void);
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
    }

    xTaskCreate(
        periodic_task_poll_buttons,     // Task function
        "PeriodicTask_PollButtons",     // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        10,                             // Task priority
        &periodicTaskHandle_btn         // Task handle
    );

    xTaskCreate(
        periodic_task_poll_encoders,    // Task function
        "PeriodicTask_PollEncoders",    // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
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

// read encoders. return direction of rotation if there is one.
uint8_t poll_encoders(void) {
    uint8_t enc_events = 0;
    uint8_t curr_encoder_states = 0;
    for(uint8_t enc=0; enc < 4; enc++) {
        curr_encoder_states |= (gpio_get_level(Enc_array[enc]) << enc);
    }

    if (curr_encoder_states != Prev_encoder_states) {   // an encoder moved, not necessarily full event
        // enc 1
        if ((curr_encoder_states & 3) == 3) {   // current state at detent
            if ((Prev_encoder_states & 3) == 1) {
                enc_events |= 0x10;     // enc1 CW
            } else if ((Prev_encoder_states & 3) == 2) {
                enc_events |= 0x20;     // enc1 CCW
            }
        }
        // enc 2
        if ((curr_encoder_states >> 2) == 3) {   // current state at detent
            if ((Prev_encoder_states >> 2) == 1) {
                enc_events |= 0x40;     // enc2 CW
            } else if ((Prev_encoder_states >> 2) == 2) {
                enc_events |= 0x80;     // enc2 CCW
            }
        }
    }
    Prev_encoder_states = curr_encoder_states;

    return (enc_events);
}


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


// Define task: Periodically poll encoders (now posts events instead of direct processing)
void periodic_task_poll_encoders(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_ENCODER_TASK_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        
        // Poll encoders and post events for any rotation
        // encoder events:  enc1 rotate CW: set bit0, rotate CCW: set bit1
        //                  enc2 rotate CW: set bit2, rotate CCW: set bit3
        uint8_t encoder_events = poll_encoders();
        
        if (encoder_events) {
            EventSystem_PostEvent(EVENT_UI_ENCODER, encoder_events, NULL);  // Encoder 0
        }
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}

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
        
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}

void setup_gpio_input(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}
