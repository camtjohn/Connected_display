#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
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
#include "mqtt_protocol.h"
#include "ui.h"
#include "led_driver.h"
#include "view.h"
#include "weather.h"
#include "local_time.h"
#include "provisioning.h"
#include "provisioning_view.h"

#include "esp_tls.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

const static char *TAG = "WEATHER_STATION: MAIN";

#define ENABLE_DEBUG_MODE   1

#define WIFI_PROVISION_BUTTON BTN_1  // Button to trigger factory provisioning

// TASK
TaskHandle_t periodicTaskHandle_sleep = NULL;
static void periodic_task_sleep_wake(void *);
TaskHandle_t periodicTaskHandle_server = NULL;
static void periodic_task_check_server(void *);
TaskHandle_t periodicTaskHandle_heartbeat = NULL;
static void periodic_task_heartbeat(void *);
static void periodic_task_wifi_reconnect(void *);
static void event_listener_task(void *);  // Handles provisioning button press

// Private helper function
static void startup_wifi(void);
static void handle_wifi_connection_failure(void);
static void start_connected_services(void);
static void on_got_ip(void* arg, esp_event_base_t base, int32_t event_id, void* event_data);

// One-time start guard for network-dependent services
static bool s_services_started = false;

void app_main(void) {
    ESP_LOGI(TAG, "Starting up");

    Led_driver__Initialize();
    View__Initialize();

    // Initialize Event System early (before UI which uses events)
    EventSystem_Initialize();

    // Initialize device config from NVS first
    Device_Config__Init();
    
    Ui__Initialize();

    // Start up WiFi (connect, register IP handler, and IP check)
    startup_wifi();
    
    EventSystem_StartTasks();

    // Start event listener task to handle provisioning button
    xTaskCreate(
        event_listener_task,            // Task function
        "EventListenerTask",            // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        6,                              // Task priority
        NULL                            // Task handle
    );

    // Main loop just sleeps - all work is done in tasks and event handlers
    while (1) {
        // Could add system health monitoring, watchdog feeding, etc. here
        vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep 10 seconds
    }
}

// PRIVATE METHODS

// Start up Wifi actions
static void startup_wifi(void) {
    // Attempt WiFi connection (will return after connect or failure)
    int wifi_result = Wifi__Start();
    
    // Register handler to start services when IP is obtained (handles late connects)
    esp_event_handler_instance_t ip_event_instance;
    esp_err_t reg_err = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &on_got_ip,
        NULL,
        &ip_event_instance
    );
    if (reg_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(reg_err));
    }
    
    // Check if WiFi connection failed
    if (wifi_result != 0) {
        ESP_LOGW(TAG, "WiFi connection failed - entering offline mode");
        handle_wifi_connection_failure();
        // Function returns if user chooses to continue without WiFi
        // or reboots to factory if user wants to provision
    }

    // Also handle the case where IP was already acquired before handler registration
    esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0) {
            ESP_LOGI(TAG, "IP already assigned; starting connected services now");
            start_connected_services();
        }
    }
    
    // WiFi startup complete - switch from bootup view to menu
    View__Set_view(VIEW_MENU);
}

// Start services that require network connectivity (idempotent)
static void start_connected_services(void) {
    if (s_services_started) return;
    OTA__Init();
    Mqtt__Start();
    // Initialize SNTP/time after network is up
    Local_Time__Init_SNTP();
    s_services_started = true;
    ESP_LOGI(TAG, "Network services started (OTA, MQTT)");

    // Start tasks that depend on MQTT/network being up
    if (periodicTaskHandle_sleep == NULL) {
        xTaskCreate(
            periodic_task_sleep_wake,       // Task function
            "PeriodicTask_SleepWake",       // Task name (for debugging)
            (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
            NULL,                           // Task parameter
            5,                              // Task priority
            &periodicTaskHandle_sleep       // Task handle
        );
    }
    if (periodicTaskHandle_server == NULL) {
        xTaskCreate(
            periodic_task_check_server,     // Task function
            "PeriodicTask_CheckServer",     // Task name (for debugging)
            (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
            NULL,                           // Task parameter
            5,                              // Task priority
            &periodicTaskHandle_server      // Task handle
        );
    }
    if (periodicTaskHandle_heartbeat == NULL) {
        xTaskCreate(
            periodic_task_heartbeat,        // Task function
            "PeriodicTask_Heartbeat",       // Task name (for debugging)
            (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
            NULL,                           // Task parameter
            5,                              // Task priority
            &periodicTaskHandle_heartbeat   // Task handle
        );
    }
}

// Event handler: called when station gets an IP address
static void on_got_ip(void* arg, esp_event_base_t base, int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "Got IP event received; starting services if needed");
    start_connected_services();
}

// Handle WiFi connection failure - show view and wait for user action
static void handle_wifi_connection_failure(void) {
    ESP_LOGI(TAG, "WiFi connection failed - displaying provision prompt");
    
    // Check if credentials exist at all
    char ssid[32] = {0};
    if (Device_Config__Get_WiFi_SSID(ssid, sizeof(ssid)) != 0 || strlen(ssid) == 0) {
        ESP_LOGW(TAG, "=================================");
        ESP_LOGE(TAG, "No WiFi credentials configured");
        ESP_LOGW(TAG, "BTN1: Enter provisioning mode");
        ESP_LOGW(TAG, "BTN4: Continue offline");
        ESP_LOGW(TAG, "Wait 60s to auto-continue offline");
        ESP_LOGW(TAG, "=================================");
        
        // Allow user to provision or continue offline (timeout after 60 seconds)
        View__Set_view(VIEW_PROVISIONING);
        Provisioning_View__Set_context(0);  // No credentials
        
        // Wait for user action or timeout (handled by view system now)
        for (int i = 0; i < 600; i++) {  // 600 x 100ms = 60 seconds
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        ESP_LOGI(TAG, "Continuing in offline mode (timeout reached or button pressed)");
        View__Set_view(VIEW_MENU);  // Return to menu
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
    View__Set_view(VIEW_PROVISIONING);
    Provisioning_View__Set_context(1);  // Has credentials but failed
    
    // Wait for user action or timeout (handled by view system now)
    for (int i = 0; i < 300; i++) {  // 300 x 100ms = 30 seconds
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


// Periodic task to retry WiFi connection in background when offline
static void periodic_task_wifi_reconnect(void *pvParameters) {
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
static void periodic_task_sleep_wake(void *pvParameters) {
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
static void periodic_task_check_server(void *pvParameters) {
    // After method runs, delay for this amount of ms
    const TickType_t active_server_check_every_ms = pdMS_TO_TICKS(CHECK_SERVER_PERIOD_MS);
    const TickType_t unresponsive_server_check_every_ms = pdMS_TO_TICKS(CHECK_UNRESPONSIVE_SERVER_PERIOD_MS);
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        if(Mqtt__Get_server_status() == 1) {
            Mqtt__Reset_server_status();
            Mqtt__Set_offline_mode(false);  // Server is responding
            vTaskDelayUntil(&time_start_task, active_server_check_every_ms);

        } else {
            // create view for server not communicating
            Weather__Set_view_comm_loss();
            Mqtt__Set_offline_mode(true);   // Server is not responding

            vTaskDelayUntil(&time_start_task, unresponsive_server_check_every_ms);
        }
        
    }
}

// Define task: Periodically send heartbeat to server
static void periodic_task_heartbeat(void *pvParameters) {
    const TickType_t heartbeat_interval = pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS);
    uint8_t heartbeat_msg[32];
    char device_name[16];
    
    while (1) {
        TickType_t time_start_task = xTaskGetTickCount();
        
        // Get device name from NVS and send heartbeat
        if (Device_Config__Get_Name(device_name, sizeof(device_name)) == 0) {
            int msg_len = mqtt_protocol_build_heartbeat(device_name, heartbeat_msg, sizeof(heartbeat_msg));
            if (msg_len > 0) {
                Mqtt__Publish(MQTT_TOPIC_HEARTBEAT, heartbeat_msg, msg_len);
                ESP_LOGI(TAG, "Sent heartbeat for device: %s", device_name);
            } else {
                ESP_LOGE(TAG, "Failed to build heartbeat message");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read device name for heartbeat");
        }
        
        vTaskDelayUntil(&time_start_task, heartbeat_interval);
    }
}


// Event listener task: Monitors system events for provisioning button
static void event_listener_task(void *pvParameters) {
    system_event_t event;
    
    while (1) {
        // Wait for events from the system event queue
        if (xQueueReceive(systemEventQueue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event.type == EVENT_UI_BUILTIN_BUTTON_LONGPRESS) {
                ESP_LOGI(TAG, "Entering WiFi provisioning mode (5-second button press)");
                
                // Display provisioning view - user can trigger provisioning or cancel
                View__Set_view(VIEW_PROVISIONING);
                Provisioning_View__Set_context(1);  // Triggered manually
            }
        }
    }
}


