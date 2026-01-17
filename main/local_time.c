#include <string.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include "local_time.h"

static const char *TAG = "LOCAL_TIME";

// Private variables
uint16_t Sleep_times[] = {TIME_SLEEP1, TIME_SLEEP2};
uint16_t Wakeup_times[] = {TIME_WAKEUP1, TIME_WAKEUP2};

// Private methods
Sleep_event_config find_assign_next_event(uint16_t);
void assign_next_event(Sleep_event_config*, uint16_t, uint16_t);

void Local_Time__Init_SNTP(void) {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    // Non-blocking/short wait: check once quickly, then continue; sync will retry in background
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(500)) != ESP_OK) {
        ESP_LOGI(TAG, "Time not synced yet (500 ms check); continuing, will sync in background");
    }

    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);  // Set timezone to EST
    tzset();
}

Sleep_event_config Local_Time__Get_next_sleep_event(void) {
    // Get the current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // ESP_LOGI(TAG, "hour: %d minute: %d\n", timeinfo.tm_hour, timeinfo.tm_min);
    uint16_t now_minutes = (60 * timeinfo.tm_hour) + timeinfo.tm_min;

    Sleep_event_config next_event  = find_assign_next_event(now_minutes);

    return next_event;
}

// Get current time and date return it as a string
void Local_Time__Get_current_time_str(char * time_str) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Format the date and time into a string
    strftime(time_str, 9, "%H:%M:%S", &timeinfo);
}

// Get string day of the week for today
uint8_t Local_Time__Get_letter_day_of_week(void) {
    uint8_t ret_val = 255;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char day_str[3];
    strftime(day_str, 4, "%a", &timeinfo);

    if(strcmp(day_str, "Sun")==0) {
        ret_val = 0;
    } else if(strcmp(day_str, "Mon")==0) {
        ret_val = 1;
    } else if(strcmp(day_str, "Tue")==0) {
        ret_val = 2;
    } else if(strcmp(day_str, "Wed")==0) {
        ret_val = 3;
    } else if(strcmp(day_str, "Thu")==0) {
        ret_val = 4;
    } else if(strcmp(day_str, "Fri")==0) {
        ret_val = 5;
    } else if(strcmp(day_str, "Sat")==0) {
        ret_val = 6;
    }
    return ret_val;
}

// Private method definitions

// Check event times
void assign_next_event(Sleep_event_config * next_event, uint16_t now_minutes, uint16_t increment) {
    // Iterate over each on/off events
    for(uint8_t i_time = 0; i_time < NUM_TIME_CONFIGS; i_time++) {
        // Check sleep event if after now and less than currently stored delay
        uint16_t event_minutes = Sleep_times[i_time] + increment;
        if(event_minutes > now_minutes) {
            if((event_minutes - now_minutes) < next_event->delay_min) {
                next_event->delay_min = (event_minutes - now_minutes);
                next_event->action = SLEEP;
            }
        }
        // Check wake event if after now and less than currently stored delay
        event_minutes = Wakeup_times[i_time] + increment;
        if(event_minutes > now_minutes) {
            if((event_minutes - now_minutes) < next_event->delay_min) {
                next_event->delay_min = (event_minutes - now_minutes);
                next_event->action = WAKEUP;
            }
        }
    }
}

// Find the nearest time and return it's action: Go to sleep OR Wake up
Sleep_event_config find_assign_next_event(uint16_t now_minutes) {
    Sleep_event_config next_event;
    next_event.action = INVALID;
    next_event.delay_min = 24 * 60;

    // iterate over each event time. find closest time that is after time now. store in "next_event"
    assign_next_event(&next_event, now_minutes, 0);

    // If still unassigned, now time is later in day than all events
    if(next_event.action == INVALID) {
        // Add a days worth of minutes to events and recalculate
        assign_next_event(&next_event, now_minutes, (60 * 24));
    }

    return next_event;
}