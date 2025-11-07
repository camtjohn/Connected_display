#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "event_system.h"

// Legacy defines (to be phased out)
#define FREQUENCY_CHECK_VIEW_UPDATES_MS     100 // check if view config changed every 100ms

//TASK
// Define the periodic interval in milliseconds
#define UI_BUTTON_TASK_PERIOD_MS (50)
#define UI_ENCODER_TASK_PERIOD_MS (20)
#define CHECK_SERVER_PERIOD_MS (2*60*60*1000) // every 2 hours = 2hr*60min*60sec*1000ms
#define CHECK_UNRESPONSIVE_SERVER_PERIOD_MS (20*60*1000) // every 20 min = 20*60sec*1000ms

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