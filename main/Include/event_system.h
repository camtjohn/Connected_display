#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

// Event System Types
typedef enum {
    EVENT_UI_BUTTON_DOWN,       // Button pressed (goes LOW)
    EVENT_UI_BUTTON_UP,         // Button released (goes HIGH)
    EVENT_UI_ENCODER,
    EVENT_UI_BUILTIN_BUTTON_LONGPRESS,  // GPIO 0 long press (5 seconds)
    EVENT_MQTT_DATA_RECEIVED,
    EVENT_BRIGHTNESS_CHANGE,
    EVENT_SYSTEM_SLEEP,
    EVENT_SYSTEM_WAKEUP
} event_type_t;

typedef struct {
    event_type_t type;
    uint32_t data;          // Button number, encoder direction, etc.
    void* payload;          // Optional additional data
    uint32_t timestamp;     // For debugging/timing
} system_event_t;

// Event System Configuration
#define EVENT_QUEUE_SIZE 20
#define EVENT_WAIT_TIMEOUT_MS 100

// Public variables - handles managed by event system
extern QueueHandle_t systemEventQueue;

// Public functions
void EventSystem_Initialize(void);
void EventSystem_PostEvent(event_type_t type, uint32_t data, void* payload);
void EventSystem_StartTasks(void);
void EventSystem_ClearQueue(void);

// Task functions (can be called by main if needed)
void event_dispatcher_task(void *pvParameters);

#endif // EVENT_SYSTEM_H