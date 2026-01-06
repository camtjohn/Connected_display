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
#include "device_config.h"
#include "wifi.h"
#include "ota.h"
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

#define WIFI_PROVISION_BUTTON BTN_1  // Button to trigger factory provisioning

// VIEW
uint16_t View_red[16];
uint16_t View_green[16];
uint16_t View_blue[16];

// TASK
TaskHandle_t periodicTaskHandle_sleep = NULL;
void periodic_task_sleep_wake(void *);
TaskHandle_t periodicTaskHandle_server = NULL;
void periodic_task_check_server(void *);

// Private helper function
static void handle_wifi_connection_failure(void);

void app_main(void) {
    ESP_LOGI(TAG, "Starting up");

    #if(ENABLE_DISPLAY) 
    View__Initialize();
    Led_driver__Initialize();
    Led_driver__Setup();
    // Create bootup screen!
    #endif

    #if(ENABLE_WIFI_MQTT)
    // Initialize device config from NVS first
    if (Device_Config__Init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize device config!");
    }
    
    // Initialize UI early so we can check button presses
    Ui__Initialize();
    
    Local_Time__Init_SNTP();
    
    // Attempt WiFi connection (will return after connect or failure)
    int wifi_result = Wifi__Start();
    
    // Check if WiFi connection failed
    if (wifi_result != 0) {
        ESP_LOGW(TAG, "WiFi connection failed - entering offline mode");
        handle_wifi_connection_failure();
        // Function returns if user chooses to continue without WiFi
        // or reboots to factory if user wants to provision
    }
    
    // WiFi connected successfully, proceed with normal startup
    OTA__Init();
    Mqtt__Start();
    #else
    Ui__Initialize();
    #endif

    // Initialize Event System
    EventSystem_Initialize();

    // TASKS
    #if(ENABLE_WIFI_MQTT)
    xTaskCreate(
        periodic_task_sleep_wake,       // Task function
        "PeriodicTask_SleepWake",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        5,                              // Task priority
        &periodicTaskHandle_sleep       // Task handle
    );

    xTaskCreate(
        periodic_task_check_server,     // Task function
        "PeriodicTask_CheckServer",     // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        5,                              // Task priority
        &periodicTaskHandle_server      // Task handle
    );
    #endif

    // Start Event System tasks and timers
    EventSystem_StartTasks();

    // Main loop just sleeps - all work is done in tasks and event handlers
    while (1) {
        // Could add system health monitoring, watchdog feeding, etc. here
        vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep 10 seconds
    }
}

// Handle WiFi connection failure - show view and wait for user action
static void handle_wifi_connection_failure(void) {
    ESP_LOGI(TAG, "WiFi connection failed - displaying offline view");
    
    // Check if credentials exist at all
    DeviceConfig *config = Device_Config__Get();
    if (config == NULL || strlen(config->wifi_ssid) == 0) {
        ESP_LOGE(TAG, "No WiFi credentials configured");
        ESP_LOGW(TAG, "=================================");
        ESP_LOGW(TAG, "No WiFi Credentials Found");
        ESP_LOGW(TAG, "BTN1: Provision WiFi");
        ESP_LOGW(TAG, "BTN4: Continue offline");
        ESP_LOGW(TAG, "=================================");
        
        // Allow user to provision or continue offline
        while (1) {
            if (Ui__Is_Button_Pressed(1)) {  // BTN_1 - Provision
                ESP_LOGI(TAG, "Rebooting to factory app for provisioning");
                vTaskDelay(pdMS_TO_TICKS(500));
                OTA__Request_Factory_Boot();
            }
            if (Ui__Is_Button_Pressed(4)) {  // BTN_4 - Continue offline
                ESP_LOGI(TAG, "Continuing offline without WiFi credentials");
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        return;  // Exit without starting retry task (no credentials to retry with)
    }
    
    // Credentials exist but connection failed (router offline, wrong password, etc.)
    ESP_LOGW(TAG, "=================================");
    ESP_LOGW(TAG, "WiFi Connection Failed");
    ESP_LOGW(TAG, "BTN1: Enter provisioning mode");
    ESP_LOGW(TAG, "BTN4: Continue offline");
    ESP_LOGW(TAG, "Wait 30s to auto-continue offline");
    ESP_LOGW(TAG, "=================================");
    
    // Wait up to 30 seconds for user action
    for (int i = 0; i < 300; i++) {  // 300 x 100ms = 30 seconds
        if (Ui__Is_Button_Pressed(1)) {  // BTN_1 - Provision
            ESP_LOGI(TAG, "User requested WiFi provisioning - rebooting to factory app");
            vTaskDelay(pdMS_TO_TICKS(500));
            OTA__Request_Factory_Boot();
            // Does not return - reboots to factory
        }
        if (Ui__Is_Button_Pressed(4)) {  // BTN_4 - Continue offline
            ESP_LOGI(TAG, "User chose to continue offline");
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Continuing without WiFi but with credentials - start background retry task
    ESP_LOGI(TAG, "Continuing in offline mode (will retry WiFi connection in background)");
    
    // Create background task to retry WiFi connection periodically
    xTaskCreate(
        periodic_task_wifi_reconnect,
        "PeriodicTask_WiFiReconnect",
        (2*configMINIMAL_STACK_SIZE),
        NULL,
        4,  // Lower priority than main tasks
        NULL
    );
}

// PRIVATE METHODS
        vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep 10 seconds
    }
}

// PRIVATE METHODS

// Periodic task to retry WiFi connection in background when offline
void periodic_task_wifi_reconnect(void *pvParameters) {
    int reconnect_delay_seconds = 5;  // Start with 5 seconds
    const int MAX_RECONNECT_DELAY = 300;  // Max 5 minutes
    
    ESP_LOGI(TAG, "WiFi reconnection task started");
    
    while (1) {
        // Try to connect
        ESP_LOGI(TAG, "Background WiFi reconnection attempt...");
        esp_wifi_connect();
        
        // Wait for delay before next attempt, with exponential backoff
        ESP_LOGI(TAG, "Next WiFi retry in %d seconds", reconnect_delay_seconds);
        vTaskDelay(pdMS_TO_TICKS(reconnect_delay_seconds * 1000));
        
        // Exponential backoff: double the delay, up to max
        reconnect_delay_seconds *= 2;
        if (reconnect_delay_seconds > MAX_RECONNECT_DELAY) {
            reconnect_delay_seconds = MAX_RECONNECT_DELAY;
        }
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


