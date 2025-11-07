#ifndef WEATHER_H
#define WEATHER_H

typedef enum {
  DAY0,
  DAY1,
  DAY2,
  NUM_VIEWS_WEATHER
} Weather_view_type;

typedef struct {
    uint8_t current_temp;
    uint8_t max_temp;
    uint8_t min_temp;
    uint8_t precip;
    uint8_t moon;
    uint8_t sunset;
  } Weather_data_type;

//PUBLIC FUNCTION
void Weather__Initialize(void);
void Weather__Get_view(uint16_t *, uint16_t *, uint16_t *);
void Weather__Update_values(uint8_t, uint8_t*, uint8_t);

void Weather__UI_Encoder_Side(uint8_t);
void Weather__UI_Encoder_Top(uint8_t);
void Weather__UI_Button(uint8_t);
void Weather__Set_view_comm_loss(void);

#endif