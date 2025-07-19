/* Multiple views: Weather, Conway Game of Life, Etch Sketch, View Selection Screens
    Each view has it's own refresh rate (how often to )
*/

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"

#include "view.h"
#include "main.h"
#include "sprite.h"
#include "animation.h"
#include "led_driver.h"
#include "menu.h"
#include "weather.h"
#include "conway.h"
#include "etchsketch.h"

static const char *TAG = "WEATHER_STATION: VIEW";

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

// Private static variables
static uint16_t View_red[16];
static uint16_t View_green[16];
static uint16_t View_blue[16];

static View_type View_current_view;
static uint8_t View_config_updated;
static uint16_t View_refresh_rate_ms;
static uint8_t Display_State;  // 0=off, 1=on
static Brightness_level Brightness;

// Byte holds state of buttons and encoders
static uint8_t UI_event;
static uint8_t UI_needs_processed;

// Private method prototypes
void build_new_view(void); 
void decrease_brightness(void);
void increase_brightness(void);


// PUBLIC METHODS

void View__Initialize() {
    View_current_view = VIEW_WEATHER;
    View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;

    UI_event = 0;
    Display_State = 1;
    Brightness = LEVEL_MIN;

    Menu__Initialize();
    Weather__Initialize();
    Conway__Initialize();
    Etchsketch__Initialize();

    build_new_view();
    View_config_updated = 1;
    UI_needs_processed = 0;
}

// // Assemble custom view
// void View__Build_view_custom(uint16_t * view) {
//     memset(view, 0, 16*2);
//    
//     // Upper-left "CJ"
//     sprite spriteC;
//     build_sprite_generic(&spriteC, 5, 3, sprite_letter_C);
//     add_sprite(view, spriteC, 0, 13);
//
//     sprite spriteJ;
//     build_sprite_generic(&spriteJ, 5, 4, sprite_letter_J);
//     add_sprite(view, spriteJ, 0, 8);
//
//     // Lower-right "AJ"
//     sprite spriteA;
//     build_sprite_generic(&spriteA, 5, 3, sprite_letter_A);
//     add_sprite(view, spriteA, 10, 5);
//
//     add_sprite(view, spriteJ, 10, 0);
//
//     // Middle heart
//     sprite spriteHeart;
//     build_sprite_generic(&spriteHeart, 6, 6, sprite_symbol_heart);
//     add_sprite(view, spriteHeart, 6, 7);
// }


// Main calls this every 100ms
// 0000 0011 LSB: indicates UI event, 2nd bit: indicates update view
uint8_t View__Get_view_variables(void) {
    uint8_t ret_val = UI_needs_processed;
    ret_val |= (View_config_updated << 1);
    return ret_val;
}

// Mqtt calls this if updated weather values
void View__Set_if_need_update_view(View_type view_updating) {
    if (view_updating == View_current_view) {
        View_config_updated = 1;
    }
}

// Main calls
uint16_t View__Get_refresh_rate(void) {
    return View_refresh_rate_ms;
}

// Main calls this when "UI_needs_processed" is TRUE
void View__Process_UI(void) {
    // First button: switch between menu and a module view
    if(UI_event & 0x01) {
        // ESP_LOGI(TAG, "btn1, current view: %d", View_current_view);
        if (View_current_view == VIEW_MENU) {
            View_current_view = Menu__Get_current_view();
            build_new_view();
        } else {
            View_current_view = VIEW_MENU;
            build_new_view();
        }
    }

    switch(View_current_view) {
    case VIEW_MENU:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Menu__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Menu__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Menu__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Menu__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Menu__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Menu__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Menu__UI_Encoder_Side(1);
        }
        break;
    case VIEW_WEATHER:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Weather__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Weather__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Weather__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Weather__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Weather__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Weather__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Weather__UI_Encoder_Side(1);
        }
        break;
    case VIEW_CONWAY:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Conway__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Conway__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Conway__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Conway__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Conway__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Conway__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Conway__UI_Encoder_Side(1);
        }
        break;
    case VIEW_ETCHSKETCH:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Etchsketch__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Etchsketch__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Etchsketch__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Etchsketch__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Etchsketch__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Etchsketch__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Etchsketch__UI_Encoder_Side(1);
        }
        break;
    case NUM_MAIN_VIEWS:
    default:
        break;
    }
    
    View_config_updated = 1;    // Force update
    UI_event = 0;
    UI_needs_processed = 0;
}

// On UI event, module can request change view
// void View__Change_view(uint8_t direction) {
//     if(direction == 0) {
//         View_current_view ++;
//         if(View_current_view == NUM_MAIN_VIEWS) {
//             View_current_view = VIEW_WEATHER;
//         }
//     } else {
//         if(View_current_view == VIEW_WEATHER) {
//             View_current_view = NUM_MAIN_VIEWS - 1;
//         } else {
//             View_current_view --;
//         }
//     }

//     View_config_updated = 1;  // force update
// }

void View__Change_brightness(uint8_t direction) {
    if(direction == 0) {
        decrease_brightness();
    } else {
        increase_brightness();
    }
}

// Called from main
void View__Update_views(void) {
    build_new_view();
    View_config_updated = 0;
    Led_driver__Update_RAM(View_red, View_green, View_blue);
}

// Set from UI module when UI event occurs
void View__Set_UI_event(uint8_t ui_event) {
    UI_event |= ui_event;
    UI_needs_processed = 1;
}

// toggle display on/off
void View__Set_display_state(uint8_t request_state) {
    // Request turn off. If already off, do nothing.
    if(request_state == 0 && Display_State == 1) {
        Display_State = 0;
        Led_driver__Toggle_LED(0);
    // Request turn on. If already on, do nothing.
    } else if(request_state == 1 && Display_State == 0) {
        Display_State = 1;
        Led_driver__Toggle_LED(1);
    }
}

// PRIVATE METHODS

void build_new_view(void) {
    // Clear view
    memset(View_red, 0, sizeof(View_red));
    memset(View_green, 0, sizeof(View_green));
    memset(View_blue, 0, sizeof(View_blue));

    // Assemble sprite array according to requested view and values

    switch (View_current_view) {
    case VIEW_MENU:
        Menu__Get_view(View_red, View_green, View_blue);
        break;
    case VIEW_WEATHER:
        View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;
        Weather__Get_view(View_red, View_green, View_blue);
        break;
    case VIEW_CONWAY:
        View_refresh_rate_ms = Conway__Get_frame(View_red, View_green, View_blue);
        break;
    case VIEW_ETCHSKETCH:
        View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;
        Etchsketch__Get_view(View_red, View_green, View_blue);
        break;
    default:
        break;
    }

    View_config_updated = 1;    // Force update
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

