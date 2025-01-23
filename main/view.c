#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"

#include "view.h"
#include "main.h"
#include "sprite.h"

//static const char *TAG = "VIEW_WEATHER";


// Private static variables
static uint16_t View_red[16];
static uint16_t View_green[16];
static uint16_t View_blue[16];
static weather_data weather_today;   // current_temp, min_temp, max_temp, precip_percent, moon phase
static uint8_t Flag_need_update_display;

// Private method prototypes
void build_main_view(void);   
void build_double_digit_sprite(uint8_t, uint8_t, uint8_t);
uint8_t update_stored_value(uint8_t*, uint8_t);


// PUBLIC METHODS

// Receive MQTT msg with view + ints
// Assemble_sprites: According to view, create sprites based on ints. Add sprites to array.
// Apply sprites to view: Add each sprite to display

void View__Initialize() {
    weather_today.current_temp = 200;
    weather_today.max_temp = 200;
    weather_today.precip = 200;
    weather_today.moon = 200;

    View__Build_view(0);
    Flag_need_update_display = 1;
}

// Update view with static values
void View__Build_view(uint8_t view_num) {
    // Clear view
    memset(View_red, 0, sizeof(View_red));
    memset(View_green, 0, sizeof(View_green));
    memset(View_blue, 0, sizeof(View_blue));

    //ESP_LOGI(TAG, "build_view called: %u", view_num);
    // Assemble sprite array according to requested view and values
    if(view_num==0) {
        build_main_view();
    }
}

// Update view if any weather values changed. Called from mqtt module when receive msg
void View__Update_view_values(uint8_t api, uint8_t* payload, uint8_t payload_len) {
    // Current temp
    if(api==0) {
        uint8_t flag_updated_val = update_stored_value(&weather_today.current_temp, payload[0]);
        // If current temp records higher than forecast max temp, update max
        if(weather_today.current_temp > weather_today.max_temp) {
            weather_today.max_temp = weather_today.current_temp;
            Flag_need_update_display = 1;
        }
        // Only modify Flag_need_update_display flag if need to set to true
        Flag_need_update_display |= flag_updated_val;

    // Forecast today
    } else if(api==1) {
        uint8_t flag_updated_val = false;
        // Modify variable to update display. Only toggle from false to true here. Never from true to false.
        flag_updated_val |= update_stored_value(&weather_today.max_temp, payload[0]);
        flag_updated_val |= update_stored_value(&weather_today.precip, payload[1]);
        flag_updated_val |= update_stored_value(&weather_today.moon, payload[2]);
        Flag_need_update_display |= flag_updated_val;
    }
}


// Get updated view. Called from main
void View__Get_views(uint16_t* view_red, uint16_t* view_green, uint16_t* view_blue) {
    //ESP_LOGI(TAG, "get_view called, update_display: %u", Flag_need_update_display);
    if(Flag_need_update_display) {
        View__Build_view(0);
        Flag_need_update_display = 0;

        for(uint8_t row=0; row<16; row++) {
            view_red[row] = View_red[row];
        }
        for(uint8_t row=0; row<16; row++) {
            view_green[row] = View_green[row];
        }
        for(uint8_t row=0; row<16; row++) {
            view_blue[row] = View_blue[row];
        }
    }
}

// // Assemble custom view
// void View__Build_view_custom(uint16_t * view) {
//     memset(view, 0, 16*2);
    
//     // Upper-left "CJ"
//     sprite spriteC;
//     build_sprite_generic(&spriteC, 5, 3, sprite_letter_C);
//     add_sprite(view, spriteC, 0, 13);

//     sprite spriteJ;
//     build_sprite_generic(&spriteJ, 5, 4, sprite_letter_J);
//     add_sprite(view, spriteJ, 0, 8);

//     // Lower-right "AJ"
//     sprite spriteA;
//     build_sprite_generic(&spriteA, 5, 3, sprite_letter_A);
//     add_sprite(view, spriteA, 10, 5);

//     add_sprite(view, spriteJ, 10, 0);

//     // Middle heart
//     sprite spriteHeart;
//     build_sprite_generic(&spriteHeart, 6, 6, sprite_symbol_heart);
//     add_sprite(view, spriteHeart, 6, 7);
// }


// PRIVATE METHODS

// Build main views according to weather values

// add sprite(views,sprite,color)
void build_main_view(void) {
    Sprite__Add_sprite(MAX_TEMP, RED, weather_today.max_temp, View_red, View_green, View_blue);
    Sprite__Add_sprite(CURRENT_TEMP, GREEN, weather_today.current_temp, View_red, View_green, View_blue);

    // Only show precip if between 1 and 100. If 100, show all diagonal symbols
    if((weather_today.precip > 0)) {
        Sprite__Add_sprite(PRECIP, BLUE, weather_today.precip, View_red, View_green, View_blue);
    }

    // Lower-left symbol: Moon icon
    if(weather_today.moon) {
        Sprite__Add_sprite(PRECIP, WHITE, weather_today.precip, View_red, View_green, View_blue);
    }
}


// Compare new val to old val. If changed, need to update view
uint8_t update_stored_value(uint8_t* stored_val, uint8_t new_val) {
    uint8_t ret_val = 0;
    if(new_val != *stored_val) {
        *stored_val = new_val;
        ret_val = 1;
    }
    return ret_val;
}