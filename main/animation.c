#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"

#include "animation.h"
#include "view.h"

static const char *TAG = "WEATHER_STATION: ANIMATION";

// Private static variables
Animation_Scene_Type Animation_scene;

// PUBLIC METHODS

void Animation__Set_scene(void) {
    uint16_t red_view[16];
    uint16_t green_view[16];
    uint16_t blue_view[16];
    memset(red_view, 0, sizeof(red_view));
    memset(green_view, 0, sizeof(green_view));
    memset(blue_view, 0, sizeof(blue_view));

    // Animation_scene.num_frames = 8;
    // Animation_scene.current_frame = 0;
    // Animation_scene.refresh_rate_ms = 1000;
    // 
    // Assemble test pattern for 8 frames of animation scene
    // Red_origin variable set to a random number between 0 and 15
    // for(uint8_t i_frame = 0; i_frame < Animation_scene.num_frames; i_frame++) {
    //     uint16_t rand_row_val = (uint8_t)(esp_random() % 0xFFFF);
    //     for(uint8_t row=0; row<16; row++) {
    //         Animation_scene.frames[i_frame].red[row] = rand_row_val << row;
    //         Animation_scene.frames[i_frame].green[row] = (rand_row_val << (row + 1)) | (rand_row_val >> (16 - row - 1));
    //         Animation_scene.frames[i_frame].blue[row] = (rand_row_val << (row + 1)) | (rand_row_val >> (16 - row - 1));
    //     }
    // }
    // 
    // Set global static buffers according to this scene just created
    // for(uint8_t i_frame = 0; i_frame < Animation_scene.num_frames; i_frame++) {
    //     for(uint8_t row=0; row<16; row++) {
    //         Animation_scene.frames[i_frame].green[row] = green_view[row];
    //     }
    //     for(uint8_t row=0; row<16; row++) {
    //         Animation_scene.frames[i_frame].green[row] = green_view[row];
    //     }
    //     for(uint8_t row=0; row<16; row++) {
    //         Animation_scene.frames[i_frame].blue[row] = blue_view[row];
    //     }
    // }
}

uint16_t Animation__Get_scene(uint16_t * view_red, uint16_t * view_green, uint16_t * view_blue) {
    // ESP_LOGI(TAG, "frame: %u\n", Animation_scene.current_frame);
    for(uint8_t row=0; row<16; row++) {
        view_red[row] = Animation_scene.frames[Animation_scene.current_frame].red[row];
    }
    for(uint8_t row=0; row<16; row++) {
        view_green[row] = Animation_scene.frames[Animation_scene.current_frame].green[row];
    }
    for(uint8_t row=0; row<16; row++) {
        view_blue[row] = Animation_scene.frames[Animation_scene.current_frame].blue[row];
    }

    // Increment current frame so next call will grab the next frame
    Animation_scene.current_frame++;
    if(Animation_scene.current_frame >= Animation_scene.num_frames) {
        Animation_scene.current_frame = 0;  // loop back to first frame
    }

    return Animation_scene.refresh_rate_ms;
}
