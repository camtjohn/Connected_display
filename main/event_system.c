#include "event_system.h"
#include "esp_log.h"
#include "view.h"
#include "main.h"

static const char *TAG = "EVENT_SYSTEM";

// Event System Variables
QueueHandle_t systemEventQueue = NULL;
SemaphoreHandle_t displayUpdateSemaphore = NULL;
TimerHandle_t displayRefreshTimer = NULL;

// Task handle for event dispatcher
static TaskHandle_t eventDispatcherTaskHandle = NULL;

// Initialize the event system
void EventSystem_Initialize(void) {
    ESP_LOGI(TAG, "Initializing Event System");

    // Create event queue
    systemEventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(system_event_t));
    if (systemEventQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create system event queue");
        return;
    }

    // Create display update semaphore
    displayUpdateSemaphore = xSemaphoreCreateBinary();
    if (displayUpdateSemaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create display update semaphore");
        return;
    }

    // Create periodic display refresh timer
    displayRefreshTimer = xTimerCreate(
        "DisplayRefreshTimer",
        pdMS_TO_TICKS(60000),  // 60 seconds default
        pdTRUE,  // Auto-reload
        NULL,
        displayRefreshTimerCallback
    );
    if (displayRefreshTimer == NULL) {
        ESP_LOGE(TAG, "Failed to create display refresh timer");
        return;
    }

    ESP_LOGI(TAG, "Event System initialized successfully");
}

// Start event system tasks
void EventSystem_StartTasks(void) {
    ESP_LOGI(TAG, "Starting Event System tasks");

    // Create event dispatcher task (highest priority for responsiveness)
    xTaskCreate(
        event_dispatcher_task,           // Task function
        "EventDispatcher",               // Task name (for debugging)
        (3*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        12,                             // Task priority (high)
        &eventDispatcherTaskHandle      // Task handle
    );

    // Start the periodic display refresh timer
    if (xTimerStart(displayRefreshTimer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start display refresh timer");
    }

    // Post initial display update event
    EventSystem_PostEvent(EVENT_TIMER_DISPLAY_REFRESH, 0, NULL);

    ESP_LOGI(TAG, "Event System tasks started");
}

// Central event dispatcher task - processes all system events
void event_dispatcher_task(void *pvParameters) {
    system_event_t event;
    
    ESP_LOGI(TAG, "Event dispatcher task started");
    
    while (1) {
        // Wait for events from the queue
        if (xQueueReceive(systemEventQueue, &event, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)) == pdTRUE) {
            ESP_LOGI(TAG, "Processing event type: %d, data: %lu", event.type, event.data);
            
            switch (event.type) {
                case EVENT_UI_BUTTON_PRESS:
                    // Convert button press to UI event format
                    View__Set_UI_event(1 << event.data);  // Set bit for button number
                    View__Process_UI();
                    // Trigger display update
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_UI_ENCODER_CW:
                    // Convert encoder event to UI format
                    View__Set_UI_event(1 << (4 + event.data));  // Encoder CW events start at bit 4
                    View__Process_UI();
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_UI_ENCODER_CCW:
                    // Convert encoder event to UI format  
                    View__Set_UI_event(1 << (5 + event.data));  // Encoder CCW events start at bit 5
                    View__Process_UI();
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_MQTT_DATA_RECEIVED:
                    // MQTT data received - may need display update
                    View__Set_if_need_update_view(VIEW_WEATHER);  // Assuming weather data
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_TIMER_DISPLAY_REFRESH:
                    // Periodic refresh timer triggered
                    ESP_LOGI(TAG, "Periodic display refresh");
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_VIEW_CHANGE_REQUEST:
                    // View change requested
                    View__Process_UI();
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_BRIGHTNESS_CHANGE:
                    // Brightness change
                    View__Change_brightness(event.data);
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_SYSTEM_SLEEP:
                    // System sleep request
                    View__Set_display_state(0);
                    break;
                    
                case EVENT_SYSTEM_WAKEUP:
                    // System wakeup request
                    View__Set_display_state(1);
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_MQTT_CONNECTED:
                    ESP_LOGI(TAG, "MQTT Connected - updating display");
                    // Could update status indicator or connection icon
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_MQTT_DISCONNECTED:
                    ESP_LOGW(TAG, "MQTT Disconnected - updating display");
                    // Could show disconnection status
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_WIFI_CONNECTED:
                case EVENT_WIFI_DISCONNECTED:
                    ESP_LOGI(TAG, "WiFi status change - updating display");
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unhandled event type: %d", event.type);
                    break;
            }
        }
    }
}

// Post event to system event queue
void EventSystem_PostEvent(event_type_t type, uint32_t data, void* payload) {
    system_event_t event = {
        .type = type,
        .data = data,
        .payload = payload,
        .timestamp = xTaskGetTickCount()
    };
    
    // Try to post event, don't block if queue is full
    if (xQueueSend(systemEventQueue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event type: %d", type);
    }
}

// Timer callback for periodic display refresh
void displayRefreshTimerCallback(TimerHandle_t xTimer) {
    EventSystem_PostEvent(EVENT_TIMER_DISPLAY_REFRESH, 0, NULL);
}