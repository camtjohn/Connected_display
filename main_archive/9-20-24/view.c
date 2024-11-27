#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"

#include "view.h"
#include "main.h"

static const char *TAG = "VIEW_WEATHER";


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
void build_sprites_main_view(sprite *);   
void build_sprite_generic(sprite*, uint8_t, uint8_t, uint8_t, uint8_t*);
void build_double_digit_sprite(sprite*, uint8_t, uint8_t, uint8_t);
void build_null_sprite(sprite*, uint8_t, uint8_t);
void add_sprites_to_view(sprite*, uint8_t);


// Private static variables
static uint16_t View_arr[16];
static uint8_t weather_today[NUM_VALUES_WEATHER_TODAY];   // current_temp, min_temp, max_temp, precip_percent, moon phase
static bool Update_display;

// PUBLIC METHODS

// Receive MQTT msg with view + ints
// Assemble_sprites: According to view, create sprites based on ints. Add sprites to array.
// Apply sprites to view: Add each sprite to display

void View__Initialize() {
    for(uint8_t i_int=0; i_int < NUM_VALUES_WEATHER_TODAY; i_int++) {
        weather_today[i_int] = 22;
    }
    View__Build_view(0);
    Update_display = true;
}

// Update view with static values
void View__Build_view(uint8_t view_num) {
    // Clear view
    memset(View_arr, 0, (16*2));
    ESP_LOGI(TAG, "build_view called: %u", view_num);
    // Assemble sprite array according to requested view and values
    if(view_num==0) {
        sprite sprites_arr[NUM_SPRITES_MAIN_VIEW] = {0};
        build_sprites_main_view(sprites_arr);
        ESP_LOGI(TAG, "row shud be 68, then 85 %u", sprites_arr[1].data[3]);
        add_sprites_to_view(sprites_arr, NUM_SPRITES_MAIN_VIEW);
    }
}

// Update static values only if they changed. Update view if any changed.
// Return: if need to update view, return true
void View__Update_values_8bit(uint8_t api, uint8_t* short_array, uint8_t arr_len) {
    // Current temp
    if(api==0) {
        // Only update if changed
        if(weather_today[0] != short_array[0]) {
            weather_today[0] = short_array[0];
            Update_display = true;
        }
    // Forecast today
    } else if(api==1) {
        for(uint8_t i_int=1; i_int < arr_len; i_int++) {
            if(weather_today[i_int] != short_array[i_int]) {
                weather_today[i_int] = short_array[i_int];
                Update_display = true;
            }
        }
    }
    // can alternatively View__Build_view here after updating values
}


// Get updated view. Called from main
void View__Get_view(uint16_t* view_main) {
    //ESP_LOGI(TAG, "get_view called, update_display: %u", Update_display);
    if(Update_display) {
        View__Build_view(0);
        Update_display = false;
        //view_main = View_arr;
        for(uint8_t row=0; row<16; row++) {
            view_main[row] = View_arr[row];
        }
        ESP_LOGI(TAG, "15throw main view %u", view_main[14]);
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
void build_sprites_main_view(sprite * sprites_arr) {
    uint8_t temp_val;

    // Upper-right double digit: Max temp
    sprite sprite1;
    temp_val = weather_today[2];
    if(temp_val == 200) {
        build_null_sprite(&sprite1, 2, 0);
    } else {
        build_double_digit_sprite(&sprite1, 2, 0, temp_val);
    }
    sprites_arr[0] = sprite1;

    // Lower-right double digit: Current temp
    sprite sprite2;
    build_double_digit_sprite(&sprite2, 10, 0, weather_today[0]);
    sprites_arr[1] = sprite2;

    // Middle-left double digit: Precip percentage
    sprite sprite3;
    build_double_digit_sprite(&sprite3, 4, 9, weather_today[3]);
    sprites_arr[2] = sprite3;

    // Precipitation diagonal line
    sprite sprite4;
    build_sprite_generic(&sprite4, 1, 9, 2, sprite_precip_line);
    sprites_arr[3] = sprite4;

    // Temp high line
    sprite sprite5;
    build_sprite_generic(&sprite5, 0, 0, 1, sprite_max_temp_line);
    sprites_arr[4] = sprite5;

    // Moon icon
    if (weather_today[5] == 2) {
        sprite sprite6;
        build_sprite_generic(&sprite6, 11, 10, 4, sprite_full_moon);
        sprites_arr[6] = sprite6;
    } else if (weather_today[5] == 1) {
        sprite sprite6;
        build_sprite_generic(&sprite6, 11, 10, 4, sprite_almost_full_moon);
        sprites_arr[6] = sprite6;
    }
}

// Build small generic sprite
void build_sprite_generic(sprite* sprite_ptr, uint8_t loc_row, uint8_t loc_col, uint8_t num_rows, uint8_t* int_array) {
    sprite_ptr->data = int_array;
    sprite_ptr->loc_row = loc_row;
    sprite_ptr->loc_col = loc_col;
    sprite_ptr->num_rows = num_rows;
}

// Extract ones and tens digit from double digit integer and apply to new double digit sprite
void build_double_digit_sprite(sprite* sprite_ptr, uint8_t loc_row, uint8_t loc_col, uint8_t double_digit_int) {
    double_digit_int %= 100;
    uint8_t dig_tens = double_digit_int / 10;
    uint8_t dig_ones = double_digit_int % 10;
    // Assemble two digits into one sprite
    uint8_t num_rows = 5;   //small number sprite has 5 rows
    uint8_t int_arr[5];
    for (uint8_t i_row=0; i_row < num_rows; i_row++) {
        uint8_t dig_ones_row = sprite_small_num[dig_ones][i_row];
        uint8_t dig_tens_row = sprite_small_num[dig_tens][i_row] << 4;
        int_arr[i_row] = dig_ones_row | dig_tens_row;
    }
    
    sprite_ptr->data = int_arr;
    sprite_ptr->loc_row = loc_row;
    sprite_ptr->loc_col = loc_col;
    sprite_ptr->num_rows = num_rows;
}

void build_null_sprite(sprite* sprite_ptr, uint8_t loc_row, uint8_t loc_col) {
    uint8_t num_rows = 5;   //small number sprite has 5 rows
    uint8_t int_arr[5];
    for (uint8_t i_row=1; i_row <= num_rows; i_row++) {
        int_arr[i_row-1] = i_row*2;
    }

    sprite_ptr->data = int_arr;
    sprite_ptr->loc_row = loc_row;
    sprite_ptr->loc_col = loc_col;
    sprite_ptr->num_rows = num_rows;
}
// Add sprite to view: offset_row from top. offset_col from right side
void add_sprites_to_view(sprite* sprites_arr, uint8_t arr_len) {
    uint8_t offset_row;
    uint8_t offset_col;
    uint8_t view_row;
    sprite sprite_temp;
    // Add each sprite in array to view
    for(uint8_t i_sprite=0; i_sprite < arr_len; i_sprite++) {
        sprite_temp = sprites_arr[i_sprite];
        if(i_sprite==1) {
            ESP_LOGI(TAG, "temp sprite row4 %u", sprite_temp.data[3]);
        }

        offset_row = sprite_temp.loc_row;
        offset_col = sprite_temp.loc_col;
        for(int i_row=0; i_row < sprite_temp.num_rows; i_row++) {
            view_row = i_row + offset_row;
            // start adding sprite at "offset_row" index of view. shift sprite to left "offset_col"
            View_arr[view_row] |= (sprite_temp.data[i_row] << offset_col);
        }
    }
}