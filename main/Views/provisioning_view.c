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
static uint8_t in_softap_mode = 0;  // 1 when in SoftAP mode waiting for provisioning

// Simple "PROV" text pattern for 4x4 LED matrix
// Displays as a pulsing indicator that device is in provisioning mode
static const uint16_t provisioning_red[16] = {
    0x0000, 0x3BB8, 0x2210, 0x3B10, 
    0x0A10, 0x3B90, 0x0000, 0x22B4, 
    0x22A4, 0x2AB4, 0x2AA4, 0x14A4, 
    0x0000, 0x1000, 0xA000, 0x4000
};

static const uint16_t provisioning_green[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x1000, 0xA000, 0x4000
};

static const uint16_t provisioning_blue[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x1005, 0xA002, 0x4005
};

// View displays network name/password
static const uint16_t provisioning_ssid_red[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x5C07, 
    0x4405, 0x5C07, 0x5005, 0x5D57, 
    0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t provisioning_ssid_green[16] = {
    0x0000, 0x4776, 0x4224, 0x4226, 
    0x4224, 0x7726, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t provisioning_ssid_blue[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0005, 0x0002, 0x0005
};

void Provisioning_View__Initialize(void) {
    ESP_LOGI(TAG, "Provisioning view initialized");
}

void Provisioning_View__Get_frame(view_frame_t *frame) {
    if (!frame) return;
    
    if (in_softap_mode) {
        // Display SSID/password view when in SoftAP mode
        memcpy(frame->red, provisioning_ssid_red, sizeof(frame->red));
        memcpy(frame->green, provisioning_ssid_green, sizeof(frame->green));
        memcpy(frame->blue, provisioning_ssid_blue, sizeof(frame->blue));
    } else {
        // Display pulsing provisioning indicator
        memcpy(frame->red, provisioning_red, sizeof(frame->red));
        memcpy(frame->green, provisioning_green, sizeof(frame->green));
        memcpy(frame->blue, provisioning_blue, sizeof(frame->blue));
    }
}

void Provisioning_View__Set_context(uint8_t context) {
    has_credentials = context;
    user_action_taken = 0;  // Reset action flag
    in_softap_mode = 0;  // Reset SoftAP mode flag
}

int Provisioning_View__Get_user_action(void) {
    return user_action_taken;
}

int Provisioning_View__Is_in_softap_mode(void) {
    return in_softap_mode;
}

void Provisioning_View__UI_Button(uint8_t btn) {
    if (in_softap_mode) {
        // In SoftAP mode - allow exit with button 4
        if (btn == 3) {  // Button 4 - Exit SoftAP mode
            ESP_LOGI(TAG, "User exiting SoftAP mode");
            user_action_taken = 1;
            in_softap_mode = 0;
            View__Set_view(VIEW_MENU);
        }
    } else {
        // In prompt mode - allow starting provisioning or continuing offline
        if (btn == 1) {  // Button 2 - Start provisioning
            ESP_LOGI(TAG, "User confirmed - entering SoftAP mode");
            user_action_taken = 1;  // Set flag
            in_softap_mode = 1;  // Enter SoftAP mode
            
            // Start provisioning in non-blocking way
            // The actual provisioning web server runs in background
            // User can exit with button 4
        } else if (btn == 3) {  // Button 4 - Continue offline
            ESP_LOGI(TAG, "User chose to continue offline");
            user_action_taken = 1;  // Set flag
            // Return to menu view
            View__Set_view(VIEW_MENU);
        }
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
