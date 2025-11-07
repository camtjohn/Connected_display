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
}

// check all buttons. return byte with bit set for buttons that are pressed
// read/modify Btn_state to track prev state
// Return: button number 1-4
void Ui__Monitor_poll_btns(void) {
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

    if(btn_events > 0) {
        View__Set_UI_event(btn_events);
    }
}

// read encoders. return direction of rotation if there is one.
void Ui__Monitor_poll_encoders(void) {
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
    if (enc_events > 0) {
        View__Set_UI_event(enc_events);
    }
}


// New event-driven functions that return state without directly processing
uint8_t Ui__Monitor_poll_btns_get_state(void) {
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

    return btn_events;
}

uint8_t Ui__Monitor_poll_encoders_get_state(void) {
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
    return enc_events;
}

// PRIVATE FUNCTIONS

void setup_gpio_input(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}
