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

static const char *TAG = "WEATHER_STATION: VIEW";

// Private static variables
static uint16_t View_red[16];
static uint16_t View_green[16];
static uint16_t View_blue[16];
static weather_data Weather_today;   // current_temp, min_temp, max_temp, precip_percent, moon phase

static View_type View_current_view;
static uint8_t View_config_updated;
static uint16_t View_refresh_rate_ms;

// Private method prototypes
void build_new_view(View_type);
void build_main_view(void);   
void build_custom_view(void);
void build_animation_view(void); 
void build_double_digit_sprite(uint8_t, uint8_t, uint8_t);
uint8_t update_stored_value(uint8_t*, uint8_t);


// PUBLIC METHODS

// Receive MQTT msg with view + ints
// Assemble_sprites: According to view, create sprites based on ints. Add sprites to array.
// Apply sprites to view: Add each sprite to display

void View__Initialize() {
    View_current_view = VIEW_WEATHER;
    View_refresh_rate_ms = MAIN_PERIOD_MS;

    Weather_today.current_temp = 200;
    Weather_today.max_temp = 200;
    Weather_today.precip = 200;
    Weather_today.moon = 200;

    Animation__Set_scene();
    build_new_view(View_current_view);
    View_config_updated = 1;
}

// Update view if any weather values changed. Called from mqtt module when receive msg
void View__Update_view_values(uint8_t api, uint8_t* payload, uint8_t payload_len) {
    // Current temp
    if(api==0) {
        View_config_updated |= update_stored_value(&Weather_today.current_temp, payload[0]);
        // If current temp records higher than forecast max temp, update max
        if(Weather_today.current_temp > Weather_today.max_temp) {
            Weather_today.max_temp = Weather_today.current_temp;
            View_config_updated |= 1;
        }

    // Forecast today
    } else if(api==1) {
        uint8_t flag_updated_val = false;
        // Modify variable to update display. Only toggle from false to true here. Never from true to false.
        flag_updated_val |= update_stored_value(&Weather_today.max_temp, payload[0]);
        flag_updated_val |= update_stored_value(&Weather_today.precip, payload[1]);
        flag_updated_val |= update_stored_value(&Weather_today.moon, payload[2]);
        View_config_updated |= flag_updated_val;
    }
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

uint8_t View__Need_update_refresh_rate(void) {
    return View_config_updated;
}

uint16_t View__Get_refresh_rate(void) {
    // ESP_LOGI(TAG, "just before return refresh to main: %u\n", View_refresh_rate_ms);
    return View_refresh_rate_ms;
}

void View__Set_view(View_type view) {
    if(view != View_current_view) {
        View_current_view = view;
        View_config_updated = 1;  // force update
    }
}

void View__Update_views(void) {
    build_new_view(View_current_view);
    View_config_updated = 0;
    Led_driver__Update_RAM(View_red, View_green, View_blue);
}


// PRIVATE METHODS

// Update view with static values
void build_new_view(View_type view_class) {
    // Clear view
    memset(View_red, 0, sizeof(View_red));
    memset(View_green, 0, sizeof(View_green));
    memset(View_blue, 0, sizeof(View_blue));

    // Assemble sprite array according to requested view and values
    if(view_class == VIEW_WEATHER) {
        build_main_view();
    } else if(view_class == VIEW_CUSTOM) {
        build_custom_view();
    } else if(view_class == VIEW_ANIMATION) {
        build_animation_view();
    }
}

// Build main views according to weather values
// add sprite(views,sprite,color)
void build_main_view(void) {
    Sprite__Add_sprite(MAX_TEMP, RED, Weather_today.max_temp, View_red, View_green, View_blue);
    Sprite__Add_sprite(CURRENT_TEMP, GREEN, Weather_today.current_temp, View_red, View_green, View_blue);

    // Only show precip if between 1 and 100. If 100, show all diagonal symbols
    if((Weather_today.precip > 0)) {
        Sprite__Add_sprite(PRECIP, BLUE, Weather_today.precip, View_red, View_green, View_blue);
        // ESP_LOGI(TAG, "precip: %u\n", Weather_today.precip);
    }

    // Lower-left symbol: Moon icon
    if(Weather_today.moon) {
        Sprite__Add_sprite(MOON, WHITE, Weather_today.moon, View_red, View_green, View_blue);
    }
}

void build_custom_view(void) {
    Sprite__Add_sprite(CUSTOM, WHITE, 0, View_red, View_green, View_blue);
}

void build_animation_view(void) {
    View_refresh_rate_ms = Animation__Get_scene(View_red, View_green, View_blue);
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
