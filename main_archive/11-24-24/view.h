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

//PUBLIC FUNCTION
void View__Initialize();
void View__Build_view(uint8_t);
void View__Update_values_8bit(uint8_t, uint8_t*, uint8_t);
//void View__Build_view_custom(uint16_t*);
void View__Get_view(uint16_t*);


#endif