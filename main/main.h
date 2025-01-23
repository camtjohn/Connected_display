#ifndef MAIN_H
#define MAIN_H

//const static char *TAG = "WEATHER_STATION";

// Public method
#define MAIN_PERIOD_MS    30000   // Update view every 30sec=30000

//TASK
// Define the periodic interval in milliseconds
#define TASK_PERIOD_MS (50)


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