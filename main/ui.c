#include "esp_log.h"
#include "driver/gpio.h"

#include "ui.h"
#include "led_driver.h"
#include "view.h"

// static const char *TAG = "UI_WEATHER";

typedef enum {
    LEVEL_MIN,
    LEVEL_2,
    LEVEL_3,
    LEVEL_4,
    LEVEL_5,
    LEVEL_6,
    LEVEL_7,
    LEVEL_8,
    LEVEL_9,
    LEVEL_10,
    LEVEL_11,
    LEVEL_12,
    LEVEL_13,
    LEVEL_14,
    LEVEL_15,
    LEVEL_MAX
} Brightness_level;

static Brightness_level Brightness;
static uint8_t Display_State;  // 0=off, 1=on
uint8_t Btn_array[4] = {BTN_1, BTN_2, BTN_3, BTN_4};
uint8_t Btn_state;  // 4th bit=BTN_4, LSB=BTN_1

void setup_gpio_input(uint8_t);
void decrease_brightness(void);
void increase_brightness(void);

// PUBLIC FUNCTIONS
void Ui__Initialize(void) {
    Display_State = 1;
    Brightness = LEVEL_MIN;
    Btn_state = 0;
    
    for(uint8_t btn=0; btn < 4; btn++) {
        uint8_t btn_num = Btn_array[btn];
        setup_gpio_input(btn_num);
    }
}

// check all buttons. return byte with bit set for buttons that are pressed
// read/modify Btn_state to track prev state
// Return: button number 1-4
uint8_t Ui__Monitor_poll_btns(void) {
    uint8_t ret_val = 0;

    // If press btn, latch and do something. Only unlatch if detect no press.
    for(uint8_t btn=0; btn < 4; btn++) {
        uint8_t btn_num = Btn_array[btn];
        uint8_t prev_state = (Btn_state >> btn) & 1;
        uint8_t curr_state = gpio_get_level(btn_num);

        if(prev_state == NOT_PRESSED) {
            if(curr_state == PRESSED) {
                Btn_state |= (1 << btn);
                ret_val = btn+1;
            }
        } else {        // previously btn pressed
            if(curr_state == NOT_PRESSED) {
                Btn_state &= ~(1 << btn);
            }
        }
    }

    return(ret_val);
}

// take action based on btn and view
void Ui__Btn_action(uint8_t btn) {
    // get view
    switch(btn) {
        case 1:
            View__Set_view(VIEW_WEATHER);  // weather
            break;
        case 2:
            View__Set_view(VIEW_ANIMATION);  // animation
            break;
        case 3:
            // action btn3
            decrease_brightness();
            break;
        case 4:
            // action btn4
            increase_brightness();
            break;
        default:
            break;  
    }
}

// toggle display on/off
void Ui__Set_display_state(uint8_t state) {
    // Request turn off. If already off, do nothing.
    if(state == 0 && Display_State == 1) {
        Display_State = 0;
        Led_driver__Toggle_LED(0);
    // Request turn on. If already on, do nothing.
    } else if(state == 1 && Display_State == 0) {
        Display_State = 1;
        Led_driver__Toggle_LED(1);
    }
}

// PRIVATE FUNCTIONS

void setup_gpio_input(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

void decrease_brightness(void) {
    // ESP_LOGI(TAG, "Brightness: %d", Brightness);
    if(Brightness == LEVEL_MIN) {
        Display_State = 0;
        Led_driver__Toggle_LED(0);
    } else {
        Brightness--;
        Led_driver__Set_brightness(Brightness);
    }
}

// When display off, Brightness==0xA0
void increase_brightness(void) {
    // ESP_LOGI(TAG, "Brightness: %d", Brightness);
    if(Display_State == 0) {
        Display_State = 1;
        Led_driver__Toggle_LED(1);
    } else if(Brightness < LEVEL_MAX) {
        Brightness++;
        Led_driver__Set_brightness(Brightness);
    }
}
