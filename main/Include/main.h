#ifndef MAIN_H
#define MAIN_H

// Firmware version compiled into this binary
#define FW_VERSION_NUM 5

// Debug build flag - uncomment to use debug topics
#define DEBUG_BUILD

#include "freertos/FreeRTOS.h"
#include "event_system.h"

//TASK
// Define the periodic interval in milliseconds
#define CHECK_SERVER_PERIOD_MS (2*60*60*1000) // every 2 hours = 2hr*60min*60sec*1000ms
#define CHECK_UNRESPONSIVE_SERVER_PERIOD_MS (20*60*1000) // every 20 min = 20*60sec*1000ms

#endif