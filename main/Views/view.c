/* Multiple views: Weather, Conway Game of Life, Etch Sketch, View Selection Screens
    Each view has it's own refresh rate (how often to )
*/

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "view.h"
#include "main.h"
#include "event_system.h"
#include "sprite.h"
#include "animation.h"
#include "led_driver.h"
#include "menu.h"
#include "weather.h"
#include "conway.h"
#include "etchsketch.h"

static const char *TAG = "WEATHER_STATION: VIEW";

typedef enum {
    LEVEL_MIN,
    LEVEL_2,
    LEVEL_3,
    LEVEL_4,
    LEVEL_5,
    LEVEL_6,
    LEVEL_7,
    LEVEL_8,
    LEVEL_9,
    LEVEL_10,
    LEVEL_11,
    LEVEL_12,
    LEVEL_13,
    LEVEL_14,
    LEVEL_15,
    LEVEL_MAX
} Brightness_level;

// Public variable
SemaphoreHandle_t displayUpdateSemaphore = NULL;

// Private static variables
static uint16_t View_red[16];
static uint16_t View_green[16];
static uint16_t View_blue[16];

static View_type View_current_view;
static volatile uint32_t View_refresh_rate_ms;
static uint8_t Display_State;  // 0=off, 1=on
static Brightness_level Brightness;

// Thread variables
TaskHandle_t blockingTaskHandle_display = NULL;
void blocking_thread_update_display(void *);

// Private method prototypes
void build_new_view(void); 
void decrease_brightness(void);
void increase_brightness(void);
void post_event(event_type_t, uint32_t);

// PUBLIC METHODS

void View__Initialize() {
    View_current_view = VIEW_WEATHER;
    Display_State = 1;
    Brightness = LEVEL_MIN;
    View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;

    Menu__Initialize();
    Weather__Initialize();
    Conway__Initialize();
    Etchsketch__Initialize();

    displayUpdateSemaphore = xSemaphoreCreateBinary();

    xTaskCreate(
        blocking_thread_update_display,       // Task function
        "BlockingTask_UpdateDisplay",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        8,                             // Task priority
        &blockingTaskHandle_display       // Task handle
    );
}

// Event system calls this to set UI event bits
// Encoding rules (16-bit):
// - BUTTON_DOWN: bits 0-3 = button 1-4, bit 8 = 0
// - BUTTON_UP:   bits 0-3 = button 1-4, bit 8 = 1 (0x100)
// - ENCODER:     bits 4-7 = encoder events (0x10, 0x20 for top; 0x40, 0x80 for side)
void View__Process_UI(uint16_t UI_event) {
    // Check if this is a button UP event using bit 8
    uint8_t button_bits = (UI_event & 0x0F);
    uint8_t is_button_up = (button_bits != 0) && (UI_event & 0x100);
    
    // Skip BUTTON_UP events for most views
    // Only etchsketch will handle UP events (to exit paint mode)
    if (is_button_up && View_current_view != VIEW_ETCHSKETCH) {
        // This is a BUTTON_UP event - ignore for menu, weather, conway
        return;
    }
    
    // First button: switch between menu and a module view (button DOWN only)
    if((UI_event & 0x01) && !is_button_up) {
        if (View_current_view == VIEW_MENU) {
            View_current_view = Menu__Get_current_view();
            build_new_view();
        } else {
            View_current_view = VIEW_MENU;
            build_new_view();
        }
    }

    switch(View_current_view) {
    case VIEW_MENU:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Menu__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Menu__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Menu__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Menu__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Menu__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Menu__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Menu__UI_Encoder_Side(1);
        }
        break;
    case VIEW_WEATHER:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Weather__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Weather__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Weather__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Weather__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Weather__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Weather__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Weather__UI_Encoder_Side(1);
        }
        break;
    case VIEW_CONWAY:
        // Buttons
        if(UI_event & 0x02) {   //btn2
            Conway__UI_Button(1);
        }
        if(UI_event & 0x04) {   //btn3
            Conway__UI_Button(2);
        }
        if(UI_event & 0x08) {   //btn4
            Conway__UI_Button(3);
        }
        //enc1
        if(UI_event & 0x10) {
            Conway__UI_Encoder_Top(0);
        } else if(UI_event & 0x20) {
            Conway__UI_Encoder_Top(1);
        }
        //enc2
        if(UI_event & 0x40) {
            Conway__UI_Encoder_Side(0);
        } else if(UI_event & 0x80) {
            Conway__UI_Encoder_Side(1);
        }
        break;
    case VIEW_ETCHSKETCH:
        if (is_button_up) {
            // Handle button release events for paint mode
            if(UI_event & 0x02) {   // btn2 released
                Etchsketch__UI_Button_Released(1);
            }
            if(UI_event & 0x04) {   // btn3 released
                Etchsketch__UI_Button_Released(2);
            }
            if(UI_event & 0x08) {   // btn4 released
                Etchsketch__UI_Button_Released(3);
            }
        } else {
            // Handle button press and encoder events (as before)
            // Buttons
            if(UI_event & 0x02) {   //btn2
                Etchsketch__UI_Button(1);
            }
            if(UI_event & 0x04) {   //btn3
                Etchsketch__UI_Button(2);
            }
            if(UI_event & 0x08) {   //btn4
                Etchsketch__UI_Button(3);
            }
            //enc1
            if(UI_event & 0x10) {
                Etchsketch__UI_Encoder_Top(0);
            } else if(UI_event & 0x20) {
                Etchsketch__UI_Encoder_Top(1);
            }
            //enc2
            if(UI_event & 0x40) {
                Etchsketch__UI_Encoder_Side(0);
            } else if(UI_event & 0x80) {
                Etchsketch__UI_Encoder_Side(1);
            }
        }
        break;
    case NUM_MAIN_VIEWS:
    default:
        break;
    }
    
    UI_event = 0;

    // ESP_LOGI(TAG, "ui updating view");
    xSemaphoreGive(displayUpdateSemaphore);
}

// Post view-related events to the event system
void post_event(event_type_t type, uint32_t data) {
    EventSystem_PostEvent(type, data, NULL);
}

void View__Change_brightness(uint8_t direction) {
    if(direction == 0) {
        decrease_brightness();
    } else {
        increase_brightness();
    }
}

// toggle display on/off
void View__Set_display_state(uint8_t request_state) {
    // Request turn off. If already off, do nothing.
    if(request_state == 0 && Display_State == 1) {
        Display_State = 0;
        Led_driver__Toggle_LED(0);
    // Request turn on. If already on, do nothing.
    } else if(request_state == 1 && Display_State == 0) {
        Display_State = 1;
        Led_driver__Toggle_LED(1);
    }
}

// PRIVATE METHODS

void build_new_view(void) {
    // Clear view
    memset(View_red, 0, sizeof(View_red));
    memset(View_green, 0, sizeof(View_green));
    memset(View_blue, 0, sizeof(View_blue));

    // Assemble sprite array according to requested view and values
    switch (View_current_view) {
    case VIEW_MENU:
        View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;
        Menu__Get_view(View_red, View_green, View_blue);
        break;
    case VIEW_WEATHER:
        View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;
        Weather__Get_view(View_red, View_green, View_blue);
        break;
    case VIEW_CONWAY:
        View_refresh_rate_ms = Conway__Get_frame(View_red, View_green, View_blue);
        break;
    case VIEW_ETCHSKETCH:
        View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;
        Etchsketch__Get_view(View_red, View_green, View_blue);
        break;
    default:
        break;
    }
}


// Led_driver functions update display
void decrease_brightness(void) {
    // ESP_LOGI(TAG, "Brightness: %d", Brightness);
    if(Brightness == LEVEL_MIN) {
        Display_State = 0;
        Led_driver__Toggle_LED(0);
    } else {
        Brightness--;
        Led_driver__Set_brightness(Brightness);
    }
}

// Led_driver functions update display
void increase_brightness(void) {
    // ESP_LOGI(TAG, "Brightness: %d", Brightness);
    if(Display_State == 0) {
        Display_State = 1;
        Led_driver__Toggle_LED(1);
    } else if(Brightness < LEVEL_MAX) {
        Brightness++;
        Led_driver__Set_brightness(Brightness);
    }
}

void blocking_thread_update_display(void *pvParameters) {
    while(1) {
        // Wait for semaphore from event system or timeout for periodic refresh
        TickType_t wait_ticks = (View_refresh_rate_ms == 0) ? DEFAULT_REFRESH_RATE_MS : pdMS_TO_TICKS(View_refresh_rate_ms);

        xSemaphoreTake(displayUpdateSemaphore, wait_ticks);
        
        // Update display (whether triggered by UI event or timeout)
        build_new_view();
        Led_driver__Update_RAM(View_red, View_green, View_blue);
    }
}