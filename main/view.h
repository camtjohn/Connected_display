#ifndef VIEW_H
#define VIEW_H

#define NUM_VALUES_WEATHER_TODAY  4   // max_temp, precip_percent, moon phase
#define NUM_SPRITES_MAIN_VIEW     6   // max_temp, current_temp, precip, moon, diag_line, straight_line

//PUBLIC TYPES
typedef struct {
  uint8_t current_temp;
  uint8_t max_temp;
  uint8_t min_temp;
  uint8_t precip;
  uint8_t moon;
  uint8_t sunset;
} weather_data;

typedef enum {
    STATIC,
    DYNAMIC
} Display_type;

typedef enum {
    VIEW_WEATHER,
    VIEW_CUSTOM,
    VIEW_ANIMATION
} View_type;

//PUBLIC FUNCTION
void View__Initialize();
void View__Update_view_values(uint8_t, uint8_t*, uint8_t);
// void View__Build_view_custom(uint16_t*);
uint8_t View__Need_update_refresh_rate(void);
uint16_t View__Get_refresh_rate(void);
void View__Set_view(View_type);
void View__Update_views(void);


#endif