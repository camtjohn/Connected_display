#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

// Event System Types
typedef enum {
    EVENT_UI_BUTTON_PRESS,
    EVENT_UI_ENCODER_CW,
    EVENT_UI_ENCODER_CCW,
    EVENT_MQTT_DATA_RECEIVED,
    EVENT_MQTT_CONNECTED,
    EVENT_MQTT_DISCONNECTED,
    EVENT_TIMER_DISPLAY_REFRESH,
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_VIEW_CHANGE_REQUEST,
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
extern SemaphoreHandle_t displayUpdateSemaphore;
extern TimerHandle_t displayRefreshTimer;

// Public functions
void EventSystem_Initialize(void);
void EventSystem_PostEvent(event_type_t type, uint32_t data, void* payload);
void EventSystem_StartTasks(void);

// Task functions (can be called by main if needed)
void event_dispatcher_task(void *pvParameters);
void displayRefreshTimerCallback(TimerHandle_t xTimer);

#endif // EVENT_SYSTEM_H