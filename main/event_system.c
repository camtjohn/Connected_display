#include "event_system.h"
#include "esp_log.h"
#include "view.h"
#include "main.h"

static const char *TAG = "EVENT_SYSTEM";

// Event System Variables
QueueHandle_t systemEventQueue = NULL;

// Task handle for event dispatcher
static TaskHandle_t eventDispatcherTaskHandle = NULL;

// Initialize the event system
void EventSystem_Initialize(void) {
    // Create event queue
    systemEventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(system_event_t));
    if (systemEventQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create system event queue");
        return;
    }

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
}

// Central event dispatcher task - processes all system events
void event_dispatcher_task(void *pvParameters) {
    system_event_t event;
    
    ESP_LOGI(TAG, "Event dispatcher task started");
    
    while (1) {
        // Wait for events from the queue
        if (xQueueReceive(systemEventQueue, &event, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)) == pdTRUE) {
            // ESP_LOGI(TAG, "Processing event type: %d, data: %lu", event.type, event.data);
            
            switch (event.type) {
                case EVENT_UI_BUTTON_DOWN:
                    // Button pressed (down)
                    for (int i = 0; i < 4; i++) {
                        if (event.data & (1 << i)) {
                            View__Process_UI(1 << i);
                        }
                    }
                    break;
                    
                case EVENT_UI_BUTTON_UP:
                    // Button released (up) - most modules ignore this
                    // Etchsketch module uses it to exit paint mode
                    for (int i = 0; i < 4; i++) {
                        if (event.data & (1 << i)) {
                            View__Process_UI((1 << i) | 0x80);  // Mark as UP with high bit
                        }
                    }
                    break;
                    
                case EVENT_UI_ENCODER:
                    // Check each encoder event and notify view
                    // first 4 bits are buttons, next 4 bits are encoders
                    for (int i = 4; i < 8; i++) {
                        if (event.data & (1 << i)) {
                            View__Process_UI(1 << i);
                        }
                    }
                    break;

                case EVENT_MQTT_DATA_RECEIVED:
                    // MQTT data received - may need display update
                    xSemaphoreGive(displayUpdateSemaphore);
                    break;
                    
                case EVENT_BRIGHTNESS_CHANGE:
                    // Brightness change
                    View__Change_brightness(event.data);
                    break;
                    
                case EVENT_SYSTEM_SLEEP:
                    // System sleep request
                    View__Set_display_state(0);
                    break;
                    
                case EVENT_SYSTEM_WAKEUP:
                    // System wakeup request
                    View__Set_display_state(1);
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