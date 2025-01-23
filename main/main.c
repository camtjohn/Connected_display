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
#include "led_driver.h"
#include "spi.h"
#include "view.h"
#include "wifi.h"
#include "mqtt.h"
#include "ui.h"

#define ENABLE_DISPLAY 1

// VIEW
uint16_t View_red[16];
uint16_t View_green[16];
uint16_t View_blue[16];


// TASK
TaskHandle_t periodicTaskHandle = NULL;
void periodic_task_poll_button(void *);
// Example suspend/resume task:
// vTaskSuspend(periodicTaskHandle);
// vTaskResume(periodicTaskHandle);

// PRIVATE METHODS

void app_main(void) {
    Wifi__Start();
    Mqtt__Start();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Done on connection in mqtt even handler: Mqtt__Subscribe(MQTT_TOPIC_DATA_UPDATE);

    #if(ENABLE_DISPLAY) 
    View__Initialize();
    Led_driver__Initialize();
    Led_driver__Setup();
    #endif

    Ui__Initialize();
    char * device_num = "0";
    Mqtt__Bootup_msgs(device_num);

    // TASK
    // Create the task with a priority of your choice
    xTaskCreate(
        periodic_task_poll_button,     // Task function
        "PeriodicTask",                 // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),       // Stack size (words)
        NULL,                           // Task parameter
        20,                              // Task priority
        &periodicTaskHandle             // Task handle
    );

    while (1) {
        View__Get_views(View_red, View_green, View_blue);
        Led_driver__Update_RAM(View_red, View_green, View_blue);

        // uint8_t btn_states = Ui__Monitor_poll_btns();
        // Ui__Btn_action(btn_states);
        vTaskDelay(pdMS_TO_TICKS(MAIN_PERIOD_MS));  //every 30sec
    }
}


// PRIVATE METHODS

// Define task: Periodically poll buttons
void periodic_task_poll_button(void *pvParameters) {
    // Initialize the xLastWakeTime variable with the current tick count before method runs
    TickType_t time_start_task = xTaskGetTickCount();
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(TASK_PERIOD_MS);
    
    while (1) {
        uint8_t btn_states = Ui__Monitor_poll_btns();
        Ui__Btn_action(btn_states);
        
        //printf("inside task...\n");
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}