#include "esp_system.h"
#include "esp_log.h"

#include "music.h"
#include "view.h"
#include "text_renderer.h"

static const char *TAG = "WEATHER_STATION: MUSIC";

// PUBLIC METHODS

void Music__Initialize(void) {
    // Nothing to initialize for now
}

void Music__Get_view(view_frame_t *frame) {
    TextRenderer__RenderString(frame, "CALM");
}

void Music__UI_Encoder_Top(uint8_t direction) {
    // For now, do nothing
}

void Music__UI_Encoder_Side(uint8_t direction) {
    // For now, do nothing
}

void Music__UI_Button(uint8_t btn) {
    // For now, do nothing
}