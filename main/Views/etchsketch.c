#include <string.h>
#include "esp_system.h"
#include "esp_log.h"

#include "etchsketch.h"
#include "view.h"

static const char *TAG = "WEATHER_STATION: ETCHSKETCH";

// Private static variables
static uint16_t View_red[16];
static uint16_t View_green[16];
static uint16_t View_blue[16];

static uint8_t Position_row;
static uint8_t Position_col;

// Private method prototypes

// PUBLIC METHODS

void Etchsketch__Initialize(void) {
    memset(View_red, 0, sizeof(View_red));
    memset(View_green, 0, sizeof(View_green));
    memset(View_blue, 0, sizeof(View_blue));

    Position_col = 0;
    Position_row = 0;
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
}

void Etchsketch__UI_Encoder_Side(uint8_t direction) {
    // Move cursor right/left
    if(direction == 0) {
        Position_row++;
        if(Position_row > 15) {
            Position_row = 0;
        }
    } else {
        if(Position_row > 0) {
            Position_row --;
        } else {
            Position_row = 15;
        }
    }
}

void Etchsketch__UI_Button(uint8_t btn) {
    // if(btn == 0) {
    if (btn ==1) {
        // ESP_LOGI(TAG, "btn red");
        View_red[Position_row] ^= (1 << Position_col);
    } else if (btn == 2) {
        // ESP_LOGI(TAG, "btn green");
        View_green[Position_row] ^= (1 << Position_col);
    } else if (btn == 3) {
        // ESP_LOGI(TAG, "btn blue");
        View_blue[Position_row] ^= (1 << Position_col);
    }
}

// PRIVATE METHODS

