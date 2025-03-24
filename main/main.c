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

#include "led_driver.h"
#include "local_time.h"
#include "main.h"
#include "mqtt.h"
#include "ui.h"
#include "view.h"
#include "wifi.h"

const static char *TAG = "WEATHER_STATION: MAIN";

#define ENABLE_DISPLAY      1
#define ENABLE_WIFI_MQTT    1
#define ENABLE_DEBUG_MODE   1

// VIEW
uint16_t View_red[16];
uint16_t View_green[16];
uint16_t View_blue[16];

// TASK
TaskHandle_t periodicTaskHandle = NULL;
void periodic_task_poll_button(void *);
TaskHandle_t periodicTaskHandle_sleep = NULL;
void periodic_task_sleep_wake(void *);


// Example suspend/resume task:
// vTaskSuspend(periodicTaskHandle);
// vTaskResume(periodicTaskHandle);

// PRIVATE METHODS

void app_main(void) {
    ESP_LOGI(TAG, "Starter up");
    #if(ENABLE_WIFI_MQTT)
    Wifi__Start();
    Mqtt__Start();
    vTaskDelay(pdMS_TO_TICKS(3000));

    char * device_num = "0";
    Mqtt__Bootup_msgs(device_num);

    Local_Time__Init_SNTP();
    #endif

    #if(ENABLE_DISPLAY) 
    View__Initialize();
    Led_driver__Initialize();
    Led_driver__Setup();
    #endif

    Ui__Initialize();

    // TASKS
    xTaskCreate(
        periodic_task_poll_button,      // Task function
        "PeriodicTask_PollButton",      // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        20,                             // Task priority
        &periodicTaskHandle             // Task handle
    );

    #if(ENABLE_WIFI_MQTT)
    xTaskCreate(
        periodic_task_sleep_wake,       // Task function
        "PeriodicTask_SleepWake",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        20,                             // Task priority
        &periodicTaskHandle_sleep       // Task handle
    );
    #endif

    // MAIN LOOP
    #if (ENABLE_DEBUG_MODE)
    Mqtt__Send_debug_msg("Start up...");
    #endif
    // Led_driver__Clear_RAM();

    // Frequent check for display type change occurs every 100ms. If not change after MAIN_PERIOD_MS, then update display anyways.
    uint16_t display_refresh_rate_ms;
    uint16_t max_count;
    uint8_t count_check_view;
    while (1) {
        // Check ever 100ms if view config changed refresh rate (animation). If not, update display according to current rate.
        display_refresh_rate_ms = View__Get_refresh_rate();
        max_count = display_refresh_rate_ms / FREQUENCY_CHECK_VIEW_UPDATES_MS;
        count_check_view = 0;
        for(uint8_t i_count = 0; i_count < max_count; i_count++) {
            count_check_view++;
            if(View__Need_update_refresh_rate() == 1) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(FREQUENCY_CHECK_VIEW_UPDATES_MS));  // every 100ms
        }
        View__Update_views();
    }
}


// PRIVATE METHODS

// Define task: Periodically poll buttons
void periodic_task_poll_button(void *pvParameters) {
    // Initialize the xLastWakeTime variable with the current tick count before method runs
    TickType_t time_start_task = xTaskGetTickCount();
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_TASK_PERIOD_MS);
    
    while (1) {
        uint8_t btn_states = Ui__Monitor_poll_btns();
        Ui__Btn_action(btn_states);
        
        //printf("inside task...\n");
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}

// Define task: Sleep/Wakeup according to configuration
void periodic_task_sleep_wake(void *pvParameters) {
    if(NUM_TIME_CONFIGS > 0) {
        while (1) {
            Sleep_event_config next_event = Local_Time__Get_next_sleep_event();
            TickType_t delay_ticks = pdMS_TO_TICKS(next_event.delay_min * 60 * 1000);

            #if (ENABLE_DEBUG_MODE)
            char delay_str[4];
            snprintf(delay_str, sizeof(delay_str), "%u", next_event.delay_min);
            char debug_str[16];
            if(next_event.action == SLEEP) {
                snprintf(debug_str, sizeof(debug_str), "sleep: %s", delay_str);
            } else if (next_event.action == WAKEUP) {
                snprintf(debug_str, sizeof(debug_str), "wakeup: %s", delay_str);
            } else {
                snprintf(debug_str, sizeof(debug_str), "goof: %s", delay_str);
            }
            Mqtt__Send_debug_msg(debug_str);
            #endif

            vTaskDelay(delay_ticks);

            if(next_event.action == SLEEP) {
                #if (ENABLE_DEBUG_MODE)
                Mqtt__Send_debug_msg("sleep");
                #endif
                Ui__Set_display_state(0);  // turn off display
            } else if(next_event.action == WAKEUP) {
                #if (ENABLE_DEBUG_MODE)
                Mqtt__Send_debug_msg("wakeup");
                #endif
                Ui__Set_display_state(1);  // turn on display
            }
        }
    }
}