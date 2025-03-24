#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"

// Public method
#define MAIN_PERIOD_MS    60000   // Update view every 60sec=60000
#define FREQUENCY_CHECK_VIEW_UPDATES_MS     100 // check if view config changed every 100ms

//TASK
// Define the periodic interval in milliseconds
#define UI_TASK_PERIOD_MS (50)

// Calculate the tick rate (assuming FreeRTOS tick rate is 1 kHz, i.e., 1 ms tick)
// #define TICKS_PER_MS (configTICK_RATE_HZ / 1000)
// #define TICKS_PER_US (TICKS_PER_MS / 1000)
// #define TASK_PERIOD_TICKS (TASK_PERIOD_US * TICKS_PER_US)

/* Store the value of key 'my_key' to NVS */
// nvs_set_u32(nvs_handle, "my_key", chosen_value);
/* Read the value of key 'my_key' from NVS */
// nvs_get_u32(nvs_handle, "my_key", &chosen_value);

// reset flash
// nvs_flash_erase();
// esp_restart();

#endif