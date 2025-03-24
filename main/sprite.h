#ifndef SPRITE_H
#define SPRITE_H

typedef enum {
    RED = 0,
    GREEN,
    BLUE,
    YELLOW,     // Red + Green
    CYAN,       // Green + Blue
    PURPLE,     // Blue + Red
    WHITE       // All 3
}COLOR_TYPE;

typedef enum {
    MAX_TEMP = 0,
    CURRENT_TEMP,
    PRECIP,
    MOON,
    CUSTOM
} SPRITE_TYPE;

void Sprite__Add_sprite(SPRITE_TYPE, COLOR_TYPE, uint8_t, uint16_t*, uint16_t*, uint16_t*);

#endif