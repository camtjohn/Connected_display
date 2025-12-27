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

const uint16_t image_conway_red[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0300, 0x0210, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0420, 0x0440, 0x0000, 0x0000, 0x0000, 0x0000};
const uint16_t image_conway_blue[16] = {
    0x0000, 0x0000, 0x07F0, 0x0808, 0x1004, 0x1002, 0x1002, 0x0944, 
    0x0A88, 0x1008, 0x1028, 0x3048, 0x1310, 0x0CE0, 0x0000, 0x0000};
const uint16_t image_conway_green[16] = {
    0x2084, 0x1848, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6140, 
    0x8282, 0x0002, 0x0001, 0x0000, 0x0000, 0x0000, 0x1010, 0x2008};

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

void Menu__Get_view(view_frame_t *frame) {    
    if (!frame) return;
    switch(Menu_current_view) {
    case VIEW_WEATHER:
        memcpy(frame->red, image_weather_red, sizeof(frame->red));
        memcpy(frame->green, image_weather_green, sizeof(frame->green));
        memcpy(frame->blue, image_weather_blue, sizeof(frame->blue));
        break;
    case VIEW_CONWAY:
        memcpy(frame->red, image_conway_red, sizeof(frame->red));
        memcpy(frame->green, image_conway_green, sizeof(frame->green));
        memcpy(frame->blue, image_conway_blue, sizeof(frame->blue));
        break;
    case VIEW_ETCHSKETCH:
        memcpy(frame->red, image_etchsketch_red, sizeof(frame->red));
        memcpy(frame->green, image_etchsketch_green, sizeof(frame->green));
        memcpy(frame->blue, image_etchsketch_blue, sizeof(frame->blue));
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

