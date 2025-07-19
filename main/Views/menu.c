#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_random.h"

#include "menu.h"
#include "view.h"

static const char *TAG = "WEATHER_STATION: MENU";

// Private static variables
static View_type Menu_current_view;

const uint16_t image_weather_red[16] = {0x0000, 0x00C0, 0x0D20, 0x1210, 0x210C, 0x2012, 0x2002, 0x1804,
                                        0x200C, 0x400A, 0x3FF1, 0x0041, 0x0041, 0x0022, 0x001C, 0x0000};
const uint16_t image_weather_blue[16] = {0x0000, 0x00C0, 0x0D20, 0x1210, 0x210C, 0x2012, 0x2002, 0x1804,
                                        0x2008, 0x4008, 0x3FF0, 0x02A00, 0x5400, 0x2A00, 0x5400, 0x0000};
const uint16_t image_weather_green[16] = {0x0000, 0x00C0, 0x0D20, 0x1210, 0x210C, 0x2012, 0x2002, 0x1804,
                                        0x200C, 0x400A, 0x3FF1, 0x0041, 0x0041, 0x0022, 0x001C, 0x0000};

const uint16_t image_conway_red[16] = {};
const uint16_t image_conway_blue[16] = {};
const uint16_t image_conway_green[16] = {0x5A, 0x005A};

const uint16_t image_etchsketch_red[16] = {0x0000, 0x0000, 0x3ffc, 0x4002, 0x4002, 0x4002, 0x4002, 0x4002,
                                            0x4002, 0x4812, 0x542A, 0x4812, 0x4002, 0x3ffc, 0x0000, 0x0000};
const uint16_t image_etchsketch_blue[16] = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                            0x0000, 0x0810, 0x1428, 0x0810, 0x0000, 0x0000, 0x0000, 0x0000};
const uint16_t image_etchsketch_green[16] = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                                            0x0000, 0x0810, 0x1428, 0x0810, 0x0000, 0x0000, 0x0000, 0x0000};

// Private method prototypes


// PUBLIC METHODS
// Menu: Show images for each category when rotate knob (weather, conway, etch...)

void Menu__Initialize(void) {
    Menu_current_view = VIEW_WEATHER;
}

void Menu__Get_view(uint16_t * view_red, uint16_t * view_green, uint16_t * view_blue) {    
    switch(Menu_current_view) {
    case VIEW_WEATHER:
        for(uint8_t row=0; row<16; row++) {
            view_red[row] = image_weather_red[row];
            view_blue[row] = image_weather_blue[row];
            view_green[row] = image_weather_green[row];
        }
        break;
    case VIEW_CONWAY:
        for(uint8_t row=0; row<16; row++) {
            view_red[row] = image_conway_red[row];
            view_blue[row] = image_conway_blue[row];
            view_green[row] = image_conway_green[row];
        }
        break;
    case VIEW_ETCHSKETCH:
        for(uint8_t row=0; row<16; row++) {
            view_red[row] = image_etchsketch_red[row];
            view_blue[row] = image_etchsketch_blue[row];
            view_green[row] = image_etchsketch_green[row];
        }
        break;
    default:
        break;
    }
}

View_type Menu__Get_current_view(void) {
    // ESP_LOGI(TAG, "getting curr view: %d", Menu_current_view);
    return Menu_current_view;
}

// Methods performed on UI events (encoder/button presses)
void Menu__UI_Encoder_Top(uint8_t direction) {
    // ESP_LOGI(TAG, "Menu: top enc= %d\n", direction);
    if(direction == 0) {
        if(Menu_current_view > VIEW_WEATHER) {
            Menu_current_view --;
        } else {
            Menu_current_view = (NUM_MAIN_VIEWS - 1);
        }
    } else {
        Menu_current_view ++;
        if(Menu_current_view == NUM_MAIN_VIEWS) {
            Menu_current_view = VIEW_WEATHER;
        }
    }
}

void Menu__UI_Encoder_Side(uint8_t direction) {
    if(direction == 0) {
        View__Change_brightness(0);
    } else {
        View__Change_brightness(1);
    }
}

void Menu__UI_Button(uint8_t btn) {
    // if (btn == 0)    handled by View module
    if (btn == 1) {   // Btn 2
        ESP_LOGI(TAG, "Menu: btn2");
        //
    } else if (btn == 2) {
        //
    } else if (btn == 3) {
        //
    } 
}
// PRIVATE METHODS

