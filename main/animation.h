#ifndef ANIMATION_H
#define ANIMATION_H

#define ANIMATION_MAX_FRAMES    10

//PUBLIC TYPES
typedef struct {
    uint16_t red[16];
    uint16_t green[16];
    uint16_t blue[16];
} Frame_type;

typedef struct {
    uint8_t num_frames;         // number of frames in scene
    uint8_t current_frame;      // current frame to display
    uint16_t refresh_rate_ms;   // time between frames
    Frame_type frames[ANIMATION_MAX_FRAMES];  // array of frames
} Animation_Scene_Type;

//PUBLIC FUNCTION
void Animation__Set_scene(void);
uint16_t Animation__Get_scene(uint16_t *, uint16_t *, uint16_t *);

#endif