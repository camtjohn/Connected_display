#ifndef VIEW_H
#define VIEW_H

#include "event_system.h"  // For event_type_t

#define NUM_VALUES_WEATHER_TODAY  4   // max_temp, precip_percent, moon phase
#define NUM_SPRITES_MAIN_VIEW     6   // max_temp, current_temp, precip, moon, diag_line, straight_line

#define DEFAULT_REFRESH_RATE_MS    60000   // Update view every 60sec=60000

SemaphoreHandle_t displayUpdateSemaphore = NULL;

// Event System Types
// Forward declared. Manually update if neededEvent System Types
typedef enum {
    EVENT_UI_BUTTON_PRESS,
    EVENT_UI_ENCODER,
    EVENT_MQTT_DATA_RECEIVED,
    EVENT_BRIGHTNESS_CHANGE,
    EVENT_SYSTEM_SLEEP,
    EVENT_SYSTEM_WAKEUP
} event_type_t;

//PUBLIC TYPES

typedef enum {
    STATIC,
    DYNAMIC
} Display_type;


typedef enum {
  VIEW_MENU=0,
  VIEW_WEATHER,
  VIEW_CONWAY,
  VIEW_ETCHSKETCH,
  NUM_MAIN_VIEWS
} View_type;

typedef enum {
  BTN1_PRESSED,
  BTN2_PRESSED,
  BTN3_PRESSED,
  BTN4_PRESSED,
  ENC1_CW,
  ENC1_CCW,
  ENC2_CW,
  ENC2_CCW,
} UI_Event_Type;

//PUBLIC FUNCTION
void View__Initialize();
void View__Process_UI(uint8_t);
void View__Set_display_state(uint8_t state);
void View__Change_brightness(uint8_t);
#endif