#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"

#include "sprite.h"

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


// PRIVATE METHOD PROTOTYPES

void add_sprite_generic_small(uint16_t*, uint8_t, uint8_t, const uint8_t*, uint8_t);
void add_sprite_double_digit(uint16_t*, uint8_t, uint8_t, uint8_t);
void add_sprite_null(uint16_t*, uint8_t, uint8_t);

// PUBLIC METHODS

void Sprite__Add_sprite(SPRITE_TYPE sprite, COLOR_TYPE color, uint8_t value, uint16_t *view_red, uint16_t *view_green, uint16_t *view_blue) {
    uint16_t *ptr_view;
    uint16_t *ptr_view2;
    uint16_t *ptr_view3;
    uint8_t num_ptrs = 1;

    switch(color) {
        case RED:
            ptr_view = view_red;
            break;
        case GREEN:
            ptr_view = view_green;
            break;
        case BLUE:
            ptr_view = view_blue;
            break;
        case YELLOW:
            ptr_view = view_red;
            ptr_view2 = view_green;
            num_ptrs = 2;
            break;
        case CYAN:
            ptr_view = view_green;
            ptr_view2 = view_blue;
            num_ptrs = 2;
            break;
        case PURPLE:
            ptr_view = view_blue;
            ptr_view2 = view_red;
            num_ptrs = 2;
            break;
        case WHITE:
            ptr_view = view_blue;
            ptr_view2 = view_red;
            ptr_view3 = view_green;
            num_ptrs = 3;
            break;
        default:
            ptr_view = view_red;
            break;
    }

    switch(sprite) {
        case MAX_TEMP:
            add_sprite_generic_small(ptr_view, 0, 0, sprite_max_temp_line, 1);          // Straight line above max temp
            add_sprite_double_digit(ptr_view, 2, 0, value);                             // Upper-right double digit: Max temp
            if(num_ptrs==2) {
                add_sprite_generic_small(ptr_view2, 0, 0, sprite_max_temp_line, 1);
                add_sprite_double_digit(ptr_view2, 2, 0, value);
            }
            break;
            
        case CURRENT_TEMP:
            add_sprite_double_digit(ptr_view, 10, 0, value);                                // Lower-right double digit: Current temp
            if(num_ptrs==2) {
                add_sprite_double_digit(ptr_view2, 10, 0, value);
            }
            break;

        case PRECIP:
            if(value == 100) {
                add_sprite_generic_small(ptr_view, 4, 9, sprite_precip_line, 2);    // Display diag lines for 100% rain
                add_sprite_generic_small(ptr_view, 7, 9, sprite_precip_line, 2);
                if(num_ptrs==2) {
                    add_sprite_generic_small(ptr_view2, 4, 9, sprite_precip_line, 2);
                    add_sprite_generic_small(ptr_view2, 7, 9, sprite_precip_line, 2);
                }
            } else if (value < 100) {
                add_sprite_double_digit(ptr_view, 4, 9, value);                     // Middle-left double digit: Precip percentage
                add_sprite_generic_small(ptr_view, 1, 9, sprite_precip_line, 2);    // Diagonal line above precip
                if(num_ptrs==2) {
                    add_sprite_double_digit(ptr_view2, 4, 9, value);
                    add_sprite_generic_small(ptr_view2, 1, 9, sprite_precip_line, 2);
                }
            }
            break;

        case MOON:
            if (value == 2) {
                add_sprite_generic_small(view_red, 11, 10, sprite_full_moon, 4);
                add_sprite_generic_small(view_green, 11, 10, sprite_full_moon, 4);
                add_sprite_generic_small(view_blue, 11, 10, sprite_full_moon, 4);
            } else if (value == 1) {
                add_sprite_generic_small(view_red, 11, 10, sprite_almost_full_moon, 4);
                add_sprite_generic_small(view_green, 11, 10, sprite_almost_full_moon, 4);
                add_sprite_generic_small(view_blue, 11, 10, sprite_almost_full_moon, 4);
            }
            break;

        default:
            //
            break;
        }
}


// PRIVATE METHODS

// Extract ones and tens digit from double digit integer and apply to new double digit sprite
void add_sprite_double_digit(uint16_t *view, uint8_t loc_row, uint8_t loc_col, uint8_t double_digit_int) {
    // Value invalid
    if(double_digit_int == 200) {
        add_sprite_null(view, loc_row, loc_col);
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
            view[view_row] |= (sprite_row << loc_col);
        }
    }
}

void add_sprite_generic_small(uint16_t *view, uint8_t loc_row, uint8_t loc_col, const uint8_t* int_arr, uint8_t num_rows) {
    // Iterate through rows of sprite, inject each row at specified coordinates
    for (uint8_t i_row=0; i_row < num_rows; i_row++) {
        uint8_t view_row = i_row + loc_row;     // Inject sprite at specified row
        view[view_row] |= (int_arr[i_row] << loc_col);
    }
}

void add_sprite_null(uint16_t *view, uint8_t loc_row, uint8_t loc_col) {
    uint8_t num_rows = 5;   //small number sprite has 5 rows
    for (uint8_t i_row=0; i_row < num_rows; i_row++) {
        uint8_t view_row = i_row + loc_row;
        view[view_row] |= (1 << (i_row + loc_col + 1));     // Diagonal line
    }
}
