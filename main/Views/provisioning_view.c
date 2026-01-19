#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "provisioning_view.h"
#include "view.h"
#include "provisioning.h"

static const char *TAG = "PROVISIONING_VIEW";

// State
static uint8_t has_credentials = 0;  // 0 = no credentials, 1 = has credentials but failed
static uint8_t user_action_taken = 0;  // 1 when user presses button to exit provisioning view

// Simple "PROV" text pattern for 4x4 LED matrix
// Displays as a pulsing indicator that device is in provisioning mode
static const uint16_t provisioning_red[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0005, 0x0002, 0x0005
};

static const uint16_t provisioning_green[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x1000, 0xA000, 0x4000
};

static const uint16_t provisioning_blue[16] = {
    0x0000, 0x3BB8, 0x2210, 0x3B10, 
    0x0A10, 0x3B90, 0x0000, 0x22B4, 
    0x22A4, 0x2AB4, 0x2AA4, 0x14A4, 
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

void Provisioning_View__Set_context(uint8_t context) {
    has_credentials = context;
    user_action_taken = 0;  // Reset action flag
}

int Provisioning_View__Get_user_action(void) {
    return user_action_taken;
}

void Provisioning_View__UI_Button(uint8_t btn) {
    if (btn == 1) {  // Button 2 (btn 1 after first button handled by view.c) - Start provisioning
        ESP_LOGI(TAG, "Starting WiFi provisioning");
        user_action_taken = 1;  // Set flag
        
        if (Provisioning__Start() == 0) {
            ESP_LOGI(TAG, "Provisioning completed successfully - rebooting");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "Provisioning failed");
            // Return to menu view
            View__Set_view(VIEW_MENU);
        }
    } else if (btn == 3) {  // Button 4 - Continue offline
        ESP_LOGI(TAG, "User chose to continue offline");
        user_action_taken = 1;  // Set flag
        // Return to menu view
        View__Set_view(VIEW_MENU);
    }
}

void Provisioning_View__UI_Encoder_Top(uint8_t direction) {
    // Not used in provisioning view
}

void Provisioning_View__UI_Encoder_Side(uint8_t direction) {
    // Brightness control
    if(direction == 0) {
        View__Change_brightness(0);
    } else {
        View__Change_brightness(1);
    }
}
