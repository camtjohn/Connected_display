#include <string.h>
#include "esp_system.h"
#include "esp_log.h"

#include "provisioning_view.h"
#include "view.h"

static const char *TAG = "PROVISIONING_VIEW";

// Simple "PROV" text pattern for 4x4 LED matrix
// Displays as a pulsing indicator that device is in provisioning mode
static const uint16_t provisioning_red[16] = {
    0x0F00, 0x0090, 0x0090, 0x0F00,  // P
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t provisioning_green[16] = {
    0x0F00, 0x0090, 0x0090, 0x0F00,  // P
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t provisioning_blue[16] = {
    0x0F00, 0x0090, 0x0090, 0x0F00,  // P
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

void Provisioning_View__Initialize(void) {
    ESP_LOGI(TAG, "Provisioning view initialized");
}

void Provisioning_View__Get_frame(view_frame_t *frame) {
    if (!frame) return;
    
    // Display pulsing provisioning indicator
    memcpy(frame->red, provisioning_red, sizeof(frame->red));
    memcpy(frame->green, provisioning_green, sizeof(frame->green));
    memcpy(frame->blue, provisioning_blue, sizeof(frame->blue));
}
