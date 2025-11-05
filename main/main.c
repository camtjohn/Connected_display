#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
// #include "driver/gpio.h"

#include "main.h"
#include "wifi.h"
#include "mqtt.h"
#include "ui.h"
#include "led_driver.h"
#include "view.h"
#include "weather.h"
#include "local_time.h"

#include "esp_tls.h"


const static char *TAG = "WEATHER_STATION: MAIN";

#define ENABLE_DISPLAY      1
#define ENABLE_WIFI_MQTT    1
#define ENABLE_DEBUG_MODE   1

// VIEW
uint16_t View_red[16];
uint16_t View_green[16];
uint16_t View_blue[16];

// TASK
TaskHandle_t periodicTaskHandle_btn = NULL;
void periodic_task_poll_buttons(void *);
TaskHandle_t periodicTaskHandle_enc = NULL;
void periodic_task_poll_encoders(void *);
TaskHandle_t periodicTaskHandle_sleep = NULL;
void periodic_task_sleep_wake(void *);
TaskHandle_t periodicTaskHandle_server = NULL;
void periodic_task_check_server(void *);


// Example suspend/resume task:
// vTaskSuspend(periodicTaskHandle);
// vTaskResume(periodicTaskHandle);

// PRIVATE METHODS

void app_main(void) {
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("mbedtls", ESP_LOG_VERBOSE);
    ESP_LOGI(TAG, "Starter up");

    #if(ENABLE_DISPLAY) 
    View__Initialize();
    Led_driver__Initialize();
    Led_driver__Setup();
    // Create bootup screen!
    #endif

    #if(ENABLE_WIFI_MQTT)
    Local_Time__Init_SNTP();
    Wifi__Start();
    Mqtt__Start();
    // Is this delay still necessary? Why??
    vTaskDelay(pdMS_TO_TICKS(3000));

    char * device_num = "0";
    Mqtt__Bootup_msgs(device_num);
    #endif

    Ui__Initialize();

    // TASKS
    xTaskCreate(
        periodic_task_poll_buttons,      // Task function
        "PeriodicTask_PollButtons",      // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        10,                             // Task priority
        &periodicTaskHandle_btn             // Task handle
    );

    xTaskCreate(
        periodic_task_poll_encoders,      // Task function
        "PeriodicTask_PollEncoders",      // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        10,                             // Task priority
        &periodicTaskHandle_enc             // Task handle
    );

    #if(ENABLE_WIFI_MQTT)
    xTaskCreate(
        periodic_task_sleep_wake,       // Task function
        "PeriodicTask_SleepWake",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        5,                             // Task priority
        &periodicTaskHandle_sleep       // Task handle
    );

    xTaskCreate(
        periodic_task_check_server,       // Task function
        "PeriodicTask_CheckServer",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        5,                             // Task priority
        &periodicTaskHandle_server       // Task handle
    );
    #endif

    // MAIN LOOP
    #if (ENABLE_DEBUG_MODE)
    Mqtt__Send_debug_msg("Start up...");
    #endif
    // Led_driver__Clear_RAM();

    uint16_t display_refresh_rate_ms;
    uint16_t max_count;
    while (1) {
        // Check every 100ms if need to update view
        // If no updates occur, update display after number of counts according to refresh rate
        display_refresh_rate_ms = View__Get_refresh_rate();     // default = 1 min
        max_count = display_refresh_rate_ms / FREQUENCY_CHECK_VIEW_UPDATES_MS;
        for(uint8_t i_count = 0; i_count < max_count; i_count++) {
            // Check for UI event and if need to update view
            uint8_t view_variables = View__Get_view_variables();
            if(view_variables & 0x1) {  // UI event
                View__Process_UI();
            }
            if (view_variables & 0x2) { // Update view
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(FREQUENCY_CHECK_VIEW_UPDATES_MS));  // every 100ms
        }

        View__Update_views();
    }
}


// PRIVATE METHODS

// Define task: Periodically poll encoders
void periodic_task_poll_encoders(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_ENCODER_TASK_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        // encoder events:  enc1 rotate CW: set bit0, rotate CCW: set bit1
        //                  enc2 rotate CW: set bit2, rotate CCW: set bit3
        Ui__Monitor_poll_encoders();
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}

// Define task: Periodically poll buttons
void periodic_task_poll_buttons(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_BUTTON_TASK_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        Ui__Monitor_poll_btns();
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
                View__Set_display_state(0);  // turn off display
            } else if(next_event.action == WAKEUP) {
                #if (ENABLE_DEBUG_MODE)
                Mqtt__Send_debug_msg("wakeup");
                #endif
                View__Set_display_state(1);  // turn on display
            }
        }
    }
}

// Define task: Periodically check that server actively communicating
// every x minutes, get status from mqtt module which sets flag when it receives an update. then reset flag.
void periodic_task_check_server(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t active_server_check_every_ms = pdMS_TO_TICKS(CHECK_SERVER_PERIOD_MS);
    const TickType_t unresponsive_server_check_every_ms = pdMS_TO_TICKS(CHECK_UNRESPONSIVE_SERVER_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        if(Mqtt__Get_server_status() == 1) {
            Mqtt__Reset_server_status();
            vTaskDelayUntil(&time_start_task, active_server_check_every_ms);

        } else {
            // create view for server not communicating
            Weather__Set_view_comm_loss();
            // also track downtime?

            vTaskDelayUntil(&time_start_task, unresponsive_server_check_every_ms);
        }
        
    }
}