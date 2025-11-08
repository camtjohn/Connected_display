# Event System Module

## Overview
The `event_system` module provides a centralized, event-driven architecture for the Connected Display project. It replaces the previous polling-based approach with an efficient, scalable event queue system.

## Files
- `main/Include/event_system.h` - Header file with types and function declarations
- `main/event_system.c` - Implementation file with all event system functionality

## Key Components

### Event Types
```c
typedef enum {
    EVENT_UI_BUTTON_PRESS,      // Button press events
    EVENT_UI_ENCODER_CW,        // Encoder clockwise rotation
    EVENT_UI_ENCODER_CCW,       // Encoder counter-clockwise rotation
    EVENT_MQTT_DATA_RECEIVED,   // New MQTT data available
    EVENT_MQTT_CONNECTED,       // MQTT connection established
    EVENT_MQTT_DISCONNECTED,    // MQTT connection lost
    EVENT_WIFI_CONNECTED,       // WiFi connected
    EVENT_WIFI_DISCONNECTED,    // WiFi disconnected
    EVENT_VIEW_CHANGE_REQUEST,  // View change requested
    EVENT_BRIGHTNESS_CHANGE,    // Brightness adjustment
    EVENT_SYSTEM_SLEEP,         // System sleep request
    EVENT_SYSTEM_WAKEUP         // System wakeup request
} event_type_t;
```

### Main Functions

#### Initialization
```c
void EventSystem_Initialize(void);
```
Creates event queues, semaphores, and timers. Call this during system startup.

#### Start Tasks
```c
void EventSystem_StartTasks(void);
```
Starts the event dispatcher task and periodic timer. Call this after all other tasks are created.

#### Post Events
```c
void EventSystem_PostEvent(event_type_t type, uint32_t data, void* payload);
```
Posts an event to the system queue. Safe to call from any task or ISR.

## Usage Examples

### From UI Module
```c
// Post button press event
EventSystem_PostEvent(EVENT_UI_BUTTON_PRESS, button_number, NULL);

// Post encoder rotation
EventSystem_PostEvent(EVENT_UI_ENCODER_CW, encoder_number, NULL);
```

### From MQTT Module
```c
// Post data received event
EventSystem_PostEvent(EVENT_MQTT_DATA_RECEIVED, 0, NULL);

// Post connection status
EventSystem_PostEvent(EVENT_MQTT_CONNECTED, 0, NULL);
```


## Architecture Benefits

1. **Decoupled Modules**: Modules don't need to know about each other directly
2. **Responsive System**: Events processed immediately with high-priority task
3. **Efficient CPU Usage**: Tasks sleep when no work is needed
4. **Scalable**: Easy to add new event types and sources
5. **Maintainable**: Clear separation of concerns

## Integration Points

### Main Module
- Calls `EventSystem_Initialize()` during startup
- Calls `EventSystem_StartTasks()` after task creation

### UI Module
- Posts button and encoder events instead of direct processing
- Uses `Ui__Monitor_poll_btns_get_state()` and `Ui__Monitor_poll_encoders_get_state()`

### MQTT Module
- Posts events for data reception and connection status changes

## Configuration

Event system configuration constants in `event_system.h`:
- `EVENT_QUEUE_SIZE`: Maximum number of queued events (default: 20)
- `EVENT_WAIT_TIMEOUT_MS`: Event dispatcher timeout (default: 100ms)
- Display refresh timer: 60 seconds (configurable in timer creation)

## Task Priority
The event dispatcher runs at priority 12 (high) to ensure responsive event processing.