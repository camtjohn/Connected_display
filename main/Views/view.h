#ifndef VIEW_H
#define VIEW_H

#define NUM_VALUES_WEATHER_TODAY  4   // max_temp, precip_percent, moon phase
#define NUM_SPRITES_MAIN_VIEW     6   // max_temp, current_temp, precip, moon, diag_line, straight_line

#define DEFAULT_REFRESH_RATE_MS    60000   // Update view every 60sec=60000

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

uint8_t View__Get_view_variables(void);
void View__Set_if_need_update_view(View_type);
uint16_t View__Get_refresh_rate(void);
void View__Set_UI_event(uint8_t);
void View__Process_UI(void);
void View__Set_display_state(uint8_t state);

void View__Update_views(void);
void View__Change_brightness(uint8_t);
// void View__Change_view(uint8_t);


#endif