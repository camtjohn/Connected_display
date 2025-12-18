#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_random.h"

#include "conway.h"
#include "view.h"

static const char *TAG = "WEATHER_STATION: CONWAY";

// Private static variables

#define CONWAY_GRID_SIZE 16
static uint16_t Refresh_rate;
static uint8_t Conway_grid[CONWAY_GRID_SIZE][CONWAY_GRID_SIZE];
static uint16_t Prev_frame[16];
static uint8_t Count_same_frames;

// Private method prototypes
void restart_grid(void);
uint8_t count_neighbors(uint8_t row, uint8_t col);
void update_grid(void);


// PUBLIC METHODS
// Conway's Game of Life

void Conway__Initialize(void) {
    restart_grid();
}

// Update view arrays with new conway frame
uint16_t Conway__Get_frame(uint16_t * view_red, uint16_t * view_green, uint16_t * view_blue) {    
    // Copy new conway grid to views Red=Alive, Blue=Just died, Green=Just born
    for(uint8_t row=0; row<16; row++) {
        for(uint8_t col=0; col<16; col++) {
            if(Conway_grid[row][col] == 1) {    // Just died
                view_blue[row] |= (1 << col);
            } else if(Conway_grid[row][col] == 2) { // Continues alive
                view_red[row] |= (1 << col);
            } else if(Conway_grid[row][col] == 3) { // Just born
                view_green[row] |= (1 << col);
            }
        }
    }

    // If the red cells are staying the same over 3 frames, re initialize grid
    uint8_t updated_buffer = 0;
    for(uint8_t row=0; row<16; row++) {
        if(view_red[row] != Prev_frame[row]) {
            updated_buffer = 1;
        }
        Prev_frame[row] = view_red[row];
    }

    if(updated_buffer == 0) {
        Count_same_frames ++;
    }

    if(Count_same_frames < 3) {
        update_grid();
    } else {
        restart_grid();
    }

    return Refresh_rate;
}

// Methods performed on UI events (encoder/button presses)
void Conway__UI_Encoder_Top(uint8_t direction) {
    //
}

void Conway__UI_Encoder_Side(uint8_t direction) {
    if(direction == 0) {
        View__Change_brightness(0);
    } else {
        View__Change_brightness(1);
    }
}

void Conway__UI_Button(uint8_t btn) {
    if (btn ==1) {   // Btn 2: Restart grid
        restart_grid();
    } else if (btn == 2) {
        // ESP_LOGI(TAG, "Btn3 rate: %d", Refresh_rate);
        if(Refresh_rate > 400) {
            Refresh_rate -= 200;
        };
    } else if (btn == 3) {
        // ESP_LOGI(TAG, "Btn4 rate: %d", Refresh_rate);
        if(Refresh_rate < 3000) {
            Refresh_rate += 200;
        };
    }
}

// PRIVATE METHODS

void restart_grid(void) {
    Count_same_frames = 0;
    Refresh_rate = 1000;
    // memset(Conway_grid, 0, sizeof(Conway_grid));
    // Initialize the grid with random values
    for(uint8_t row=0; row<CONWAY_GRID_SIZE; row++) {
        for(uint8_t col=0; col<CONWAY_GRID_SIZE; col++) {
            if(esp_random() % 2) {
                Conway_grid[row][col] = 2;  // alive
            } else {
                Conway_grid[row][col] = 0;  // dead
            };
        }
    }
}

uint8_t count_neighbors(uint8_t row, uint8_t col) {
    uint8_t count = 0;
    for (uint8_t i = row - 1; i <= row + 1; i++) {
        for (uint8_t j = col - 1; j <= col + 1; j++) {
            if ((i < CONWAY_GRID_SIZE) &&       // neighbor is within boundary, negative 1 is 255
                (j < CONWAY_GRID_SIZE) &&       // neighbor is within boundary
                (i != row || j != col) &&           // neighbor is not the cell itself
                (Conway_grid[i][j] > 1)) {          // neighbor is alive
                count++;
            }
        }
    }
    return count;
}

void update_grid(void) {
    uint8_t new_grid[CONWAY_GRID_SIZE][CONWAY_GRID_SIZE];
    for (uint8_t row = 0; row < CONWAY_GRID_SIZE; row++) {
        for (uint8_t col = 0; col < CONWAY_GRID_SIZE; col++) {
            uint8_t neighbors = count_neighbors(row, col);
            // Cell was alive
            if (Conway_grid[row][col] > 1) { 
                if ((neighbors == 2) || (neighbors == 3)) {
                    // Cell continues living = Red (2)
                    new_grid[row][col] = 2;
                } else {
                    // Cell dies = Blue (1)
                    new_grid[row][col] = 1;
                }
            // Cell was dead
            } else {
                if (neighbors == 3) {
                    // Cell comes back alive = Green (3)
                    new_grid[row][col] = 3;
                } else {
                    // Cell remains dead = Black (0)
                    new_grid[row][col] = 0;
                }
            }
        }
    }
    memcpy(Conway_grid, new_grid, sizeof(Conway_grid));
}
