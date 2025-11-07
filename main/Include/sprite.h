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
    LETTER,
    CUSTOM
} SPRITE_TYPE;

typedef struct {
    uint8_t num_cols;
    uint8_t num_rows;
    uint16_t* data_arr;
} Sprite_generic;

void Sprite__Add_sprite(SPRITE_TYPE, COLOR_TYPE, uint8_t, uint16_t*, uint16_t*, uint16_t*);

#endif