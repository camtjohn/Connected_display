#include "esp_system.h"
#include "esp_log.h"

#include "weather.h"
#include "view.h"
#include "sprite.h"
#include "local_time.h"

static const char *TAG = "WEATHER_STATION: WEATHER";

// PRIVATE VARIABLES
static Weather_view_type Internal_view_type;
static Weather_data_type Weather_today;   // current_temp, min_temp, max_temp, precip_percent, moon phase
static Weather_data_type Weather_tomorrow;   // max_temp, precip_percent, moon phase
static Weather_data_type Weather_next_day;   // max_temp, precip_percent, moon phase

// PRIVATE METHODS
void build_view(uint16_t *, uint16_t *, uint16_t *);
uint8_t update_stored_value(uint8_t*, uint8_t);

// PUBLIC METHODS

void Weather__Initialize(void) {
    Internal_view_type = DAY0;

    Weather_today.current_temp = 200;
    Weather_today.max_temp = 200;
    Weather_today.precip = 200;
    Weather_today.moon = 200;
}

void Weather__Get_view(uint16_t * view_red, uint16_t * view_green, uint16_t * view_blue) {
    build_view(view_red, view_green, view_blue);
}

// Update view if any weather values changed. Called from mqtt module when receive msg
void Weather__Update_values(uint8_t api, uint8_t* payload, uint8_t payload_len) {
    uint8_t update_view = 0;
    // Current temp
    if(api==0) {
        update_view = update_stored_value(&Weather_today.current_temp, payload[0]);
        // If current temp records higher than forecast max temp, update max
        if(Weather_today.current_temp > Weather_today.max_temp) {
            Weather_today.max_temp = Weather_today.current_temp;
            update_view = 1;    // could be current temp just received did not update, but need update max
        }

    // Forecast
    } else if(api==1) {
        update_view = update_stored_value(&Weather_today.max_temp, payload[1]);
        update_view |= update_stored_value(&Weather_today.precip, payload[2]);
        update_view |= update_stored_value(&Weather_today.moon, payload[3]);
        // ESP_LOGI(TAG, "max temp: %d", Weather_today.max_temp);
        // ESP_LOGI(TAG, "precip: %d", Weather_today.precip);
        // ESP_LOGI(TAG, "moon: %d", Weather_today.moon);

        update_view |= update_stored_value(&Weather_tomorrow.max_temp, payload[4]);
        update_view |= update_stored_value(&Weather_tomorrow.precip, payload[5]);
        update_view |= update_stored_value(&Weather_tomorrow.moon, payload[6]);

        update_view |= update_stored_value(&Weather_next_day.max_temp, payload[7]);
        update_view |= update_stored_value(&Weather_next_day.precip, payload[8]);
        update_view |= update_stored_value(&Weather_next_day.moon, payload[9]);
    }

    if (update_view) {
        View__Set_if_need_update_view(VIEW_WEATHER);
    }
}

// Update view if lose communication with server. Called from mqtt module when detect comm. loss.
void Weather__Set_view_comm_loss(void) {
    Weather_today.current_temp = 200;
    Weather_today.max_temp = 200;
    Weather_today.precip = 200;
    Weather_today.moon = 200;
    Weather_tomorrow.max_temp = 200;
    Weather_tomorrow.precip = 200;
    Weather_tomorrow.moon = 200;
    Weather_next_day.max_temp = 200;
    Weather_next_day.precip = 200;
    Weather_next_day.moon = 200;
    View__Set_if_need_update_view(VIEW_WEATHER);
}

void Weather__UI_Encoder_Top(uint8_t direction) {
    // Switch internal views (forecast days)
    if(direction == 0) {
        if(Internal_view_type > DAY0) {
            Internal_view_type --;
        } else {
            Internal_view_type = (NUM_VIEWS_WEATHER - 1);
        }
        
    } else {
        Internal_view_type ++;
        if(Internal_view_type == NUM_VIEWS_WEATHER) {
            Internal_view_type = DAY0;
        }
    }
}

void Weather__UI_Encoder_Side(uint8_t direction) {
    if(direction == 0) {
        View__Change_brightness(0);
    } else {
        View__Change_brightness(1);
    }
}

void Weather__UI_Button(uint8_t btn) {
    if (btn ==1) {          // Second button (first handled by view module)
    } else if (btn == 2) {  // Third Button
        // ESP_LOGI(TAG, "Weather: btn3");
    } else if (btn == 3) {  // Fourth Button
        // ESP_LOGI(TAG, "Weather: btn4");
    }
}

// PRIVATE METHODS

void build_view(uint16_t * view_red, uint16_t * view_green, uint16_t * view_blue) {
    if(Internal_view_type == DAY0) {
        Sprite__Add_sprite(MAX_TEMP, RED, Weather_today.max_temp, view_red, view_green, view_blue);
        Sprite__Add_sprite(CURRENT_TEMP, GREEN, Weather_today.current_temp, view_red, view_green, view_blue);

        // Only show precip if between 1 and 100. If 100, show all diagonal symbols
        if((Weather_today.precip > 0)) {
            Sprite__Add_sprite(PRECIP, BLUE, Weather_today.precip, view_red, view_green, view_blue);
        }

        // Lower-left symbol: Moon icon
        if(Weather_today.moon > 0) {
            Sprite__Add_sprite(MOON, WHITE, Weather_today.moon, view_red, view_green, view_blue);
        }
        
    } else if(Internal_view_type == DAY1) {
        Sprite__Add_sprite(MAX_TEMP, RED, Weather_tomorrow.max_temp, view_red, view_green, view_blue);
        // Get number corresponding to day of the week of today (0=Sun, 1=Mon...)
        uint8_t day_of_week = Local_Time__Get_letter_day_of_week();
        if (day_of_week < 7) {  // valid day of week
            day_of_week ++;     // displaying tomorrow's day of week
            if (day_of_week > 6) {
                day_of_week = 0;
            }
            Sprite__Add_sprite(LETTER, GREEN, day_of_week, view_red, view_green, view_blue);
        }
        
        // Only show precip if between 1 and 100. If 100, show all diagonal symbols
        if((Weather_tomorrow.precip > 0)) {
            Sprite__Add_sprite(PRECIP, BLUE, Weather_tomorrow.precip, view_red, view_green, view_blue);
            // ESP_LOGI(TAG, "precip: %u\n", Weather_tomorrow.precip);
        }

        // Lower-left symbol: Moon icon
        if(Weather_tomorrow.moon) {
            Sprite__Add_sprite(MOON, WHITE, Weather_tomorrow.moon, view_red, view_green, view_blue);
        }
    
    } else if(Internal_view_type == DAY2) {
        Sprite__Add_sprite(MAX_TEMP, RED, Weather_next_day.max_temp, view_red, view_green, view_blue);
        // Get number corresponding to day of the week of today (0=Sun, 1=Mon...)
        uint8_t day_of_week = Local_Time__Get_letter_day_of_week();
        if (day_of_week < 7) {  // valid day of week
            day_of_week = day_of_week + 2;     // displaying day after tomorrow
            if (day_of_week > 6) {
                day_of_week = day_of_week - 7;
            }
            Sprite__Add_sprite(LETTER, GREEN, day_of_week, view_red, view_green, view_blue);
        }
        
        // Only show precip if between 1 and 100. If 100, show all diagonal symbols
        if((Weather_next_day.precip > 0)) {
            Sprite__Add_sprite(PRECIP, BLUE, Weather_next_day.precip, view_red, view_green, view_blue);
        }

        // Lower-left symbol: Moon icon
        if(Weather_next_day.moon) {
            Sprite__Add_sprite(MOON, WHITE, Weather_next_day.moon, view_red, view_green, view_blue);
        }
    }
}

// Compare new val to old val. If changed, need to update view
uint8_t update_stored_value(uint8_t* stored_val, uint8_t new_val) {
    uint8_t ret_val = 0;
    if(new_val != *stored_val) {
        *stored_val = new_val;
        ret_val = 1;
    }
    return ret_val;
}
