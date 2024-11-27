#ifndef MAIN_H
#define MAIN_H

//const static char *TAG = "WEATHER_STATION";


// Public method
#define UPDATE_VIEW_EVERY_MS    30000   // Update view every 30sec=30000


//TASK
// Define the periodic interval in microseconds
#define TASK_PERIOD_US (5000)

// Set refresh rate for Leds
// Less than 10 causes crash. Higher number more dim. More than 25 is too blinky
#define REFRESH_LEDS_MS     25

// Calculate the tick rate (assuming FreeRTOS tick rate is 1 kHz, i.e., 1 ms tick)
#define TICKS_PER_MS (configTICK_RATE_HZ / 1000)
#define TICKS_PER_US (TICKS_PER_MS / 1000)
#define TASK_PERIOD_TICKS (TASK_PERIOD_US * TICKS_PER_US)

#endif