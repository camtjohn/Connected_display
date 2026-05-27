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
#include "music.h"
#include "provisioning_view.h"
#include "bootup_view.h"

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
static view_frame_t View_frame;

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

typedef void (*view_render_fn)(view_frame_t *frame);
typedef void (*view_init_fn)(void);
typedef void (*view_button_fn)(uint8_t btn);
typedef void (*view_encoder_fn)(uint8_t direction);
typedef void (*view_enter_fn)(void);
typedef uint32_t (*view_refresh_fn)(void);

typedef struct {
    view_init_fn initialize;
    view_render_fn render;
    view_button_fn on_button;
    view_button_fn on_button_released;
    view_encoder_fn on_encoder_top;
    view_encoder_fn on_encoder_side;
    view_enter_fn on_enter;
    uint32_t fixed_refresh_ms;
    view_refresh_fn get_refresh_ms;
    uint8_t button_map_down[4];
    uint8_t button_map_up[4];
    uint8_t use_menu_toggle_on_btn1;
} view_module_t;

static const view_module_t *get_current_module(void);
static void process_module_buttons(const view_module_t *module, uint16_t UI_event, uint8_t is_button_up);
static void process_module_encoders(const view_module_t *module, uint16_t UI_event);
static void switch_between_menu_and_selected_view(void);

static const view_module_t View_modules[NUM_MAIN_VIEWS] = {
    [VIEW_MENU] = {
        .initialize = Menu__Initialize,
        .render = Menu__Get_view,
        .on_button = Menu__UI_Button,
        .on_encoder_top = Menu__UI_Encoder_Top,
        .on_encoder_side = Menu__UI_Encoder_Side,
        .fixed_refresh_ms = DEFAULT_REFRESH_RATE_MS,
        .button_map_down = {0, 1, 2, 3},
        .button_map_up = {0, 0, 0, 0},
        .use_menu_toggle_on_btn1 = 1,
    },
    [VIEW_WEATHER] = {
        .initialize = Weather__Initialize,
        .render = Weather__Get_view,
        .on_button = Weather__UI_Button,
        .on_encoder_top = Weather__UI_Encoder_Top,
        .on_encoder_side = Weather__UI_Encoder_Side,
        .fixed_refresh_ms = DEFAULT_REFRESH_RATE_MS,
        .button_map_down = {0, 1, 2, 3},
        .button_map_up = {0, 0, 0, 0},
        .use_menu_toggle_on_btn1 = 1,
    },
    [VIEW_CONWAY] = {
        .initialize = Conway__Initialize,
        .render = Conway__Get_frame,
        .on_button = Conway__UI_Button,
        .on_encoder_top = Conway__UI_Encoder_Top,
        .on_encoder_side = Conway__UI_Encoder_Side,
        .get_refresh_ms = Conway__Get_refresh_rate_ms,
        .button_map_down = {0, 1, 2, 3},
        .button_map_up = {0, 0, 0, 0},
        .use_menu_toggle_on_btn1 = 1,
    },
    [VIEW_ETCHSKETCH] = {
        .initialize = Etchsketch__Initialize,
        .render = Etchsketch__Get_view,
        .on_button = Etchsketch__UI_Button,
        .on_button_released = Etchsketch__UI_Button_Released,
        .on_encoder_top = Etchsketch__UI_Encoder_Top,
        .on_encoder_side = Etchsketch__UI_Encoder_Side,
        .on_enter = Etchsketch__On_Enter,
        .fixed_refresh_ms = DEFAULT_REFRESH_RATE_MS,
        .button_map_down = {0, 1, 2, 3},
        .button_map_up = {0, 1, 2, 3},
        .use_menu_toggle_on_btn1 = 1,
    },
    [VIEW_MUSIC] = {
        .initialize = Music__Initialize,
        .render = Music__Get_view,
        .on_button = Music__UI_Button,
        .on_encoder_top = Music__UI_Encoder_Top,
        .on_encoder_side = Music__UI_Encoder_Side,
        .fixed_refresh_ms = DEFAULT_REFRESH_RATE_MS,
        .button_map_down = {0, 1, 2, 3},
        .button_map_up = {0, 0, 0, 0},
        .use_menu_toggle_on_btn1 = 1,
    },
    [VIEW_PROVISIONING] = {
        .initialize = Provisioning_View__Initialize,
        .render = Provisioning_View__Get_frame,
        .on_button = Provisioning_View__UI_Button,
        .on_encoder_top = Provisioning_View__UI_Encoder_Top,
        .on_encoder_side = Provisioning_View__UI_Encoder_Side,
        .fixed_refresh_ms = DEFAULT_REFRESH_RATE_MS,
        .button_map_down = {1, 0, 0, 3},
        .button_map_up = {0, 0, 0, 0},
        .use_menu_toggle_on_btn1 = 0,
    },
    [VIEW_BOOTUP] = {
        .initialize = Bootup_View__Initialize,
        .render = Bootup_View__Get_frame,
        .fixed_refresh_ms = 500,
        .button_map_down = {0, 0, 0, 0},
        .button_map_up = {0, 0, 0, 0},
        .use_menu_toggle_on_btn1 = 1,
    },
};

// PUBLIC METHODS

void View__Initialize() {
    View_current_view = VIEW_BOOTUP;
    Display_State = 1;
    Brightness = LEVEL_MIN;
    View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;

    for (uint8_t i = 0; i < NUM_MAIN_VIEWS; i++) {
        if (View_modules[i].initialize) {
            View_modules[i].initialize();
        }
    }

    displayUpdateSemaphore = xSemaphoreCreateBinary();

    xTaskCreate(
        blocking_thread_update_display,       // Task function
        "BlockingTask_UpdateDisplay",       // Task name (for debugging)
        (2*configMINIMAL_STACK_SIZE),   // Stack size (words)
        NULL,                           // Task parameter
        8,                             // Task priority
        &blockingTaskHandle_display       // Task handle
    );

    xSemaphoreGive(displayUpdateSemaphore);
}

// Event system calls this to set UI event bits
// Encoding rules (16-bit):
// - BUTTON_DOWN: bits 0-3 = button 1-4, bit 8 = 0
// - BUTTON_UP:   bits 0-3 = button 1-4, bit 8 = 1 (0x100)
// - ENCODER:     bits 4-7 = encoder events (0x10, 0x20 for top; 0x40, 0x80 for side)
void View__Process_UI(uint16_t UI_event) {
    uint8_t button_bits = (UI_event & 0x0F);
    uint8_t is_button_up = (button_bits != 0) && (UI_event & 0x100);
    const view_module_t *module = get_current_module();

    if (!module) {
        return;
    }

    // First button toggles menu for views that opt-in (button DOWN only).
    if((UI_event & 0x01) && !is_button_up && module->use_menu_toggle_on_btn1) {
        switch_between_menu_and_selected_view();
        return;
    }

    process_module_buttons(module, UI_event, is_button_up);

    if (!is_button_up) {
        process_module_encoders(module, UI_event);
    }

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

// Switch to a specific view and update display immediately
void View__Set_view(View_type view) {
    if (view < NUM_MAIN_VIEWS) {
        View_current_view = view;
        xSemaphoreGive(displayUpdateSemaphore);  // Trigger immediate display update
    }
}

// PRIVATE METHODS

void build_new_view(void) {
    // Clear view
    memset(&View_frame, 0, sizeof(View_frame));

    const view_module_t *module = get_current_module();
    if (!module) {
        return;
    }

    if (module->render) {
        module->render(&View_frame);
    }

    if (module->get_refresh_ms) {
        View_refresh_rate_ms = module->get_refresh_ms();
    } else if (module->fixed_refresh_ms > 0) {
        View_refresh_rate_ms = module->fixed_refresh_ms;
    } else {
        View_refresh_rate_ms = DEFAULT_REFRESH_RATE_MS;
    }
}

static const view_module_t *get_current_module(void) {
    if (View_current_view >= NUM_MAIN_VIEWS) {
        return NULL;
    }
    return &View_modules[View_current_view];
}

static void process_module_buttons(const view_module_t *module, uint16_t UI_event, uint8_t is_button_up) {
    uint8_t button_bits = (UI_event & 0x0F);
    const uint8_t *button_map = is_button_up ? module->button_map_up : module->button_map_down;
    view_button_fn callback = is_button_up ? module->on_button_released : module->on_button;

    if (!button_bits || !callback) {
        return;
    }

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t bit = (1 << i);
        uint8_t mapped_button = button_map[i];
        if ((button_bits & bit) && mapped_button) {
            callback(mapped_button);
        }
    }
}

static void process_module_encoders(const view_module_t *module, uint16_t UI_event) {
    if (module->on_encoder_top) {
        if(UI_event & 0x10) {
            module->on_encoder_top(0);
        } else if(UI_event & 0x20) {
            module->on_encoder_top(1);
        }
    }

    if (module->on_encoder_side) {
        if(UI_event & 0x40) {
            module->on_encoder_side(0);
        } else if(UI_event & 0x80) {
            module->on_encoder_side(1);
        }
    }
}

static void switch_between_menu_and_selected_view(void) {
    if (View_current_view == VIEW_MENU) {
        View_current_view = Menu__Get_current_view();
    } else {
        View_current_view = VIEW_MENU;
    }

    build_new_view();
    Led_driver__Update_RAM(&View_frame);

    const view_module_t *module = get_current_module();
    if (module && module->on_enter) {
        module->on_enter();
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
        Led_driver__Update_RAM(&View_frame);
    }
}