#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "main.h"
#include "led_display.h"
#include "spi.h"
#include "view.h"
#include "wifi.h"
#include "mqtt.h"

#define ENABLE_DISPLAY 1

// VIEW
// PUBLIC VARIABLE
uint16_t view_main[16];

// TASK
TaskHandle_t periodicTaskHandle = NULL;
void periodic_task_display_view(void *);
// Example suspend/resume task:
// vTaskSuspend(periodicTaskHandle);
// vTaskResume(periodicTaskHandle);


void app_main(void) {
    Wifi__Start();
    Mqtt__Start();
    //Done on connection in mqtt even handler: Mqtt__Subscribe(MQTT_TOPIC_DATA_UPDATE);

    #if(ENABLE_DISPLAY) 
    Led_display__Initialize();
    View__Initialize();

    // TASK
    // Create the task with a priority of your choice
    xTaskCreate(
        periodic_task_display_view,          // Task function
        "PeriodicTask",        // Task name (for debugging)
        configMINIMAL_STACK_SIZE, // Stack size (words)
        NULL,                  // Task parameter
        5,  // Task priority
        &periodicTaskHandle    // Task handle
    );
    #endif

    while (1) {
        View__Get_view(view_main);
        vTaskDelay(pdMS_TO_TICKS(UPDATE_VIEW_EVERY_MS));  //every 10min
    }
}


// PRIVATE METHODS

// Define task: Continuously display view unless task paused/suspended
void periodic_task_display_view(void *pvParameters) {
    // Initialize the xLastWakeTime variable with the current tick count before method runs
    TickType_t time_start_task = xTaskGetTickCount();
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(REFRESH_LEDS_MS);
    
    while (1) {
        // printf("inside task looping...\n");
        Led_display__Display_view(view_main);
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}