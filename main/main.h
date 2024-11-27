#ifndef MAIN_H
#define MAIN_H

//const static char *TAG = "WEATHER_STATION";

// Public method
#define UPDATE_VIEW_EVERY_MS    30000   // Update view every 30sec=30000

//TASK
// Define the periodic interval in microseconds
#define TASK_PERIOD_MS (30)

// UI
#define BTN_1       12
#define BTN_2       13
#define BTN_3       14
#define BTN_4       15


// Calculate the tick rate (assuming FreeRTOS tick rate is 1 kHz, i.e., 1 ms tick)
// #define TICKS_PER_MS (configTICK_RATE_HZ / 1000)
// #define TICKS_PER_US (TICKS_PER_MS / 1000)
// #define TASK_PERIOD_TICKS (TASK_PERIOD_US * TICKS_PER_US)

#endif