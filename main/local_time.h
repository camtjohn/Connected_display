#ifndef LOCAL_TIME_H
#define LOCAL_TIME_H

#define NUM_TIME_CONFIGS        2

// Num minutes 0=12am, (23*60)=11pm
#define TIME_SLEEP1             (1)         // 12am (0)
#define TIME_WAKEUP1            (6 * 60)    // 6am (6)
#define TIME_SLEEP2             (10 * 60)   // 10am (10)
#define TIME_WAKEUP2            (17 * 60)   // 5pm (17)

// Public types
typedef enum {
    SLEEP,
    WAKEUP,
    INVALID
} Action_type;

typedef struct {
    Action_type action;
    uint16_t delay_min;
} Sleep_event_config;

// Public methods
void Local_Time__Init_SNTP(void);
Sleep_event_config Local_Time__Get_next_sleep_event(void);
void Local_Time__Get_current_time_str(char *);
uint8_t Local_Time__Get_letter_day_of_week(void);

#endif