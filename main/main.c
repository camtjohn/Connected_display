#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
// #include "driver/gpio.h"

#include "main.h"
#include "event_system.h"
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
TaskHandle_t blockingTaskHandle_display = NULL;
void blocking_task_update_display(void *);



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

    // Initialize Event System
    EventSystem_Initialize();

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

    #if(ENABLE_DISPLAY) 
    xTaskCreate(
        blocking_task_update_display,       // Task function
        "BlockingTask_UpdateDisplay",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        8,                             // Task priority
        &blockingTaskHandle_display       // Task handle
    );
    #endif

    // Start Event System tasks and timers
    EventSystem_StartTasks();

    // MAIN LOOP - Now simplified, just handles system-level operations
    #if (ENABLE_DEBUG_MODE)
    Mqtt__Send_debug_msg("Start up...");
    #endif

    // Main loop just sleeps - all work is done in tasks and event handlers
    while (1) {
        // Could add system health monitoring, watchdog feeding, etc. here
        vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep 10 seconds
    }
}


// PRIVATE METHODS

// Define task: Periodically poll encoders (now posts events instead of direct processing)
void periodic_task_poll_encoders(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_ENCODER_TASK_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        
        // Poll encoders and post events for any rotation
        // encoder events:  enc1 rotate CW: set bit0, rotate CCW: set bit1
        //                  enc2 rotate CW: set bit2, rotate CCW: set bit3
        uint8_t encoder_events = Ui__Monitor_poll_encoders_get_state();
        
        // Check encoder 1 (using correct bit positions from UI module)
        if (encoder_events & 0x10) {  // Enc1 CW (bit 4)
            EventSystem_PostEvent(EVENT_UI_ENCODER_CW, 0, NULL);  // Encoder 0
        }
        if (encoder_events & 0x20) {  // Enc1 CCW (bit 5)
            EventSystem_PostEvent(EVENT_UI_ENCODER_CCW, 0, NULL);  // Encoder 0
        }
        
        // Check encoder 2
        if (encoder_events & 0x40) {  // Enc2 CW (bit 6)
            EventSystem_PostEvent(EVENT_UI_ENCODER_CW, 1, NULL);  // Encoder 1
        }
        if (encoder_events & 0x80) {  // Enc2 CCW (bit 7)
            EventSystem_PostEvent(EVENT_UI_ENCODER_CCW, 1, NULL);  // Encoder 1
        }
        
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}

// Define task: Periodically poll buttons (now posts events instead of direct processing)
void periodic_task_poll_buttons(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(UI_BUTTON_TASK_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        
        // Poll buttons and post events for any pressed buttons
        // Note: This would ideally be replaced with interrupt-driven GPIO handling
        uint8_t button_states = Ui__Monitor_poll_btns_get_state();
        
        // Check each button and post events for pressed ones
        for (int i = 0; i < 4; i++) {  // Assuming 4 buttons
            if (button_states & (1 << i)) {
                EventSystem_PostEvent(EVENT_UI_BUTTON_PRESS, i, NULL);
            }
        }
        
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

// Define task: Update display, wait for binary semaphore from view module when need to update
void blocking_task_update_display(void *pvParameters) {
    while (1) {
        // Wait indefinitely for semaphore signal
        if (xSemaphoreTake(displayUpdateSemaphore, portMAX_DELAY) == pdTRUE) {
            // Semaphore received, update the display
            ESP_LOGI(TAG, "Display update task triggered");
            View__Update_views();
        }
    }
}

