#ifndef VIEW_H
#define VIEW_H

#define NUM_VALUES_WEATHER_TODAY  5   // current_temp, min_temp, max_temp, precip_percent, moon phase
#define NUM_SPRITES_MAIN_VIEW     6   // max_temp, current_temp, precip, moon, diag_line, straight_line

//PUBLIC TYPES
typedef struct {
  uint8_t *data;
  uint8_t loc_row;
  uint8_t loc_col;
  uint8_t num_rows;
} sprite;

typedef struct {
  uint16_t *data;
  uint8_t loc_row;
  uint8_t loc_col;
} sprite_large;

//PUBLIC FUNCTION
void View__Initialize();
void View__Build_view(uint8_t);
void View__Update_values_8bit(uint8_t, uint8_t*, uint8_t);
//void View__Build_view_custom(uint16_t*);
void View__Get_view(uint16_t*);


#endif