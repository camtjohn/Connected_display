#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_random.h"

#include "bootup_view.h"
#include "view.h"

static const char *TAG = "BOOTUP_VIEW";

// Random colorful bootup pattern - regenerated on each display update
static uint16_t bootup_red[16];
static uint16_t bootup_green[16];
static uint16_t bootup_blue[16];

void Bootup_View__Initialize(void) {
    ESP_LOGI(TAG, "Bootup view initialized");
}

void Bootup_View__Get_frame(view_frame_t *frame) {
    if (!frame) return;
    
    // Generate random colorful pixels for LED display
    for (uint8_t i = 0; i < 16; i++) {
        frame->red[i] = esp_random() & 0xFFFF;
        frame->green[i] = esp_random() & 0xFFFF;
        frame->blue[i] = esp_random() & 0xFFFF;
    }
}

void Bootup_View__UI_Button(uint8_t btn) {
    // Bootup view ignores button input
    (void)btn;
}

void Bootup_View__UI_Encoder_Top(uint8_t direction) {
    // Bootup view ignores encoder input
    (void)direction;
}

void Bootup_View__UI_Encoder_Side(uint8_t direction) {
    // Bootup view ignores encoder input
    (void)direction;
}
