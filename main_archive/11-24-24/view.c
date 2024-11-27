#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"

#include "view.h"
#include "main.h"

//static const char *TAG = "VIEW_WEATHER";


/* Sprite location in View specified by top right bit of sprite
 View origin taken from top right, increment down and to the left

 Sprite location            View
    x x x o                 ... x x x x (0,1) (0,0)
    x x x x                 ... x x x x (1,1) (1,0)    
    x x x x                 ... x x x x (2,1) (2,0)   
    x x x x                 ... 
    x x x x                 (15,15) ...       (15,0)                
*/

// Define sprites
const uint8_t sprite_small_num[10][5] = { {7, 5, 5, 5, 7},
                                    {1, 1, 1, 1, 1},
                                    {7, 1, 7, 4, 7},
                                    {7, 1, 3, 1, 7},
                                    {5, 5, 7, 1, 1},
                                    {7, 4, 7, 1, 7},
                                    {7, 4, 7, 5, 7},
                                    {7, 1, 2, 4, 4},
                                    {7, 5, 7, 5, 7},
                                    {7, 5, 7, 1, 7} };

const uint8_t sprite_up_arrow[6] = {4, 14, 21, 4, 4, 4};
const uint8_t sprite_down_arrow[6] = {4, 4, 4, 21, 14, 4};

const uint8_t sprite_letter_A[6] = {2, 5, 7, 5, 5};
const uint8_t sprite_letter_C[6] = {3, 4, 4, 4, 3};
const uint8_t sprite_letter_J[6] = {7, 2, 2, 10, 6};

const uint8_t sprite_symbol_heart[6] = {34, 85, 73, 34, 20, 8};   //dim: 6x7

const uint8_t sprite_precip_line[2] = {42, 85};   //dim: 2x7
const uint8_t sprite_max_temp_line[1] = {127};    //dim: 1x7

const uint8_t sprite_almost_full_moon[4] = {6, 9, 9, 6};   //dim: 4x4
const uint8_t sprite_full_moon[4] = {6, 15, 15, 6};    //dim: 4x4



// Private method prototypes
void build_main_view(void);   
void build_double_digit_sprite(uint8_t, uint8_t, uint8_t);
void add_sprite_generic_small(uint8_t, uint8_t, const uint8_t*, uint8_t);
void add_sprite_double_digit(uint8_t, uint8_t, uint8_t);
void add_sprite_null(uint8_t, uint8_t);
uint8_t update_weather_val(uint8_t*, uint8_t);


// Private static variables
static uint16_t View_arr[16];
static weather_data weather_today;   // current_temp, min_temp, max_temp, precip_percent, moon phase
static uint8_t Update_display;

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
    Update_display = 1;
}

// Update view with static values
void View__Build_view(uint8_t view_num) {
    // Clear view
    memset(View_arr, 0, (16*2));
    //ESP_LOGI(TAG, "build_view called: %u", view_num);
    // Assemble sprite array according to requested view and values
    if(view_num==0) {
        build_main_view();
    }
}

// Update static values only if they changed. Update view if any changed.
// Return: if need to update view, return true
void View__Update_values_8bit(uint8_t api, uint8_t* int_array, uint8_t arr_len) {
    // Current temp
    if(api==0) {
        uint8_t flag_updated_val = update_weather_val(&weather_today.current_temp, int_array[0]);
        // If current temp records higher than forecast max temp, update max
        if(weather_today.current_temp > weather_today.max_temp) {
            weather_today.max_temp = weather_today.current_temp;
        }
        // Only modify Update_display flag if need to set to true
        Update_display |= flag_updated_val;

    // Forecast today
    } else if(api==1) {
        uint8_t flag_updated_val = false;
        // Modify variable to update display. Only toggle from false to true here. Never from true to false.
        flag_updated_val |= update_weather_val(&weather_today.max_temp, int_array[0]);
        flag_updated_val |= update_weather_val(&weather_today.precip, int_array[1]);
        flag_updated_val |= update_weather_val(&weather_today.moon, int_array[2]);
        Update_display |= flag_updated_val;
    }
}


// Get updated view. Called from main
void View__Get_view(uint16_t* view_main) {
    //ESP_LOGI(TAG, "get_view called, update_display: %u", Update_display);
    if(Update_display) {
        View__Build_view(0);
        Update_display = 0;
        //view_main = View_arr;
        for(uint8_t row=0; row<16; row++) {
            view_main[row] = View_arr[row];
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

// Build sprites according to values and add to sprite array
void build_main_view(void) {
    add_sprite_generic_small(0, 0, sprite_max_temp_line, 1);    // Straight line above max temp
    add_sprite_double_digit(2, 0, weather_today.max_temp);      // Upper-right double digit: Max temp
    add_sprite_double_digit(10, 0, weather_today.current_temp); // Lower-right double digit: Current temp

    // Only show precip if between 1 and 100. If 100, show all diagonal symbols
    if((weather_today.precip > 0)) {
        if(weather_today.precip == 100) {
            add_sprite_generic_small(4, 9, sprite_precip_line, 2);
            add_sprite_generic_small(7, 9, sprite_precip_line, 2);
        } else if (weather_today.precip < 100) {
            add_sprite_double_digit(4, 9, weather_today.precip);        // Middle-left double digit: Precip percentage
            add_sprite_generic_small(1, 9, sprite_precip_line, 2);      // Diagonal line above precip
        }
    }

    // Lower-left symbol: Moon icon
    if (weather_today.moon == 2) {
        add_sprite_generic_small(11, 10, sprite_full_moon, 4);
    } else if (weather_today.moon == 1) {
        add_sprite_generic_small(11, 10, sprite_almost_full_moon, 4);
    }
}

// Extract ones and tens digit from double digit integer and apply to new double digit sprite
void add_sprite_double_digit(uint8_t loc_row, uint8_t loc_col, uint8_t double_digit_int) {
    // Value invalid
    if(double_digit_int == 200) {
        add_sprite_null(loc_row, loc_col);
    // Value valid
    } else {
        uint8_t grab_first_two_digs = double_digit_int % 100;
        uint8_t dig_tens = grab_first_two_digs / 10;
        uint8_t dig_ones = grab_first_two_digs % 10;

        // Assemble two digits into one sprite
        uint8_t num_rows = 5;   //small number sprite has 5 rows
        // Iterate through rows of sprite. Combine both digits and add to view.
        for (uint8_t i_row=0; i_row < num_rows; i_row++) {
            uint8_t dig_ones_row = sprite_small_num[dig_ones][i_row];
            uint8_t dig_tens_row = sprite_small_num[dig_tens][i_row] << 4;
            uint8_t sprite_row = dig_ones_row | dig_tens_row;   // Combine both digits into a single sprite
            uint8_t view_row = i_row + loc_row;     // Inject sprite at specified row
            View_arr[view_row] |= (sprite_row << loc_col);
        }
    }
}

void add_sprite_generic_small(uint8_t loc_row, uint8_t loc_col, const uint8_t* int_arr, uint8_t num_rows) {
    // Iterate through rows of sprite, inject each row at specified coordinates
    for (uint8_t i_row=0; i_row < num_rows; i_row++) {
        uint8_t view_row = i_row + loc_row;     // Inject sprite at specified row
        View_arr[view_row] |= (int_arr[i_row] << loc_col);
    }
}

void add_sprite_null(uint8_t loc_row, uint8_t loc_col) {
    uint8_t num_rows = 5;   //small number sprite has 5 rows
    for (uint8_t i_row=0; i_row < num_rows; i_row++) {
        uint8_t view_row = i_row + loc_row;
        View_arr[view_row] |= (1 << (i_row + loc_col + 1));     // Diagonal line
    }
}

uint8_t update_weather_val(uint8_t* stored_val, uint8_t new_val) {
    uint8_t ret_val = 0;
    if(new_val != *stored_val) {
        *stored_val = new_val;
        ret_val = 1;
    }
    return ret_val;
}