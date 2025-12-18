#include <string.h>
#include "esp_system.h"
#include "esp_log.h"

#include "etchsketch.h"
#include "view.h"
#include "ui.h"

static const char *TAG = "WEATHER_STATION: ETCHSKETCH";

// Private static variables
static uint16_t View_red[16];
static uint16_t View_green[16];
static uint16_t View_blue[16];

static uint8_t Position_row;
static uint8_t Position_col;

// Paint mode tracking
static uint8_t Paint_mode_active;      // 0=off, 1=paint with button, 2=paint with button2, 3=paint with button3
static uint8_t Paint_color;            // 0=red, 1=green, 2=blue

// Private method prototypes

// PUBLIC METHODS

void Etchsketch__Initialize(void) {
    memset(View_red, 0, sizeof(View_red));
    memset(View_green, 0, sizeof(View_green));
    memset(View_blue, 0, sizeof(View_blue));

    Position_col = 0;
    Position_row = 0;
    Paint_mode_active = 0;
    Paint_color = 0;
}

void Etchsketch__Get_view(uint16_t * view_red, uint16_t * view_green, uint16_t * view_blue) {
    for(uint8_t row=0; row<16; row++) {
        view_red[row] = View_red[row];
        view_green[row] = View_green[row];
        view_blue[row] = View_blue[row];
    }
    
    view_red[Position_row] |= (1 << Position_col);
}


// Methods performed on UI events (encoder/button presses)
void Etchsketch__UI_Encoder_Top(uint8_t direction) {
    // Move cursor right/left
    if(direction == 0) {
        Position_col++;
        if(Position_col > 15) {
            Position_col = 0;
        }
    } else {
        if(Position_col > 0) {
            Position_col --;
        } else {
            Position_col = 15;
        }
    }
    
    // If paint mode active, paint at new position
    if (Paint_mode_active) {
        if (Paint_color == 0) {
            View_red[Position_row] |= (1 << Position_col);
        } else if (Paint_color == 1) {
            View_green[Position_row] |= (1 << Position_col);
        } else if (Paint_color == 2) {
            View_blue[Position_row] |= (1 << Position_col);
        }
        ESP_LOGI(TAG, "Paint trail at row=%d col=%d (color=%d)", Position_row, Position_col, Paint_color);
    }
}

void Etchsketch__UI_Encoder_Side(uint8_t direction) {
    // Move cursor up/down
    if(direction == 0) {
        Position_row++;
        if(Position_row > 15) {
            Position_row = 0;
        }
    } else {
        if(Position_row > 0) {
            Position_row--;
        } else {
            Position_row = 15;
        }
    }
    
    // If paint mode active, paint at new position
    if (Paint_mode_active) {
        if (Paint_color == 0) {
            View_red[Position_row] |= (1 << Position_col);
        } else if (Paint_color == 1) {
            View_green[Position_row] |= (1 << Position_col);
        } else if (Paint_color == 2) {
            View_blue[Position_row] |= (1 << Position_col);
        }
        ESP_LOGI(TAG, "Paint trail at row=%d col=%d (color=%d)", Position_row, Position_col, Paint_color);
    }
}

void Etchsketch__UI_Button(uint8_t btn) {
    // Button press: enter paint mode and paint current position
    if (btn == 1) {
        Paint_mode_active = 1;
        Paint_color = 0;  // Red
        View_red[Position_row] |= (1 << Position_col);
        ESP_LOGI(TAG, "Button 1 pressed - Paint mode active (RED) at row=%d col=%d", Position_row, Position_col);
    } else if (btn == 2) {
        Paint_mode_active = 1;
        Paint_color = 1;  // Green
        View_green[Position_row] |= (1 << Position_col);
        ESP_LOGI(TAG, "Button 2 pressed - Paint mode active (GREEN) at row=%d col=%d", Position_row, Position_col);
    } else if (btn == 3) {
        Paint_mode_active = 1;
        Paint_color = 2;  // Blue
        View_blue[Position_row] |= (1 << Position_col);
        ESP_LOGI(TAG, "Button 3 pressed - Paint mode active (BLUE) at row=%d col=%d", Position_row, Position_col);
    }
}

// Button released: exit paint mode
void Etchsketch__UI_Button_Released(uint8_t btn) {
    // Exit paint mode on any button release
    if (Paint_mode_active) {
        ESP_LOGI(TAG, "Button %d released - Paint mode inactive", btn);
        Paint_mode_active = 0;
    }
}

