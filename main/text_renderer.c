#include <string.h>
#include "text_renderer.h"

typedef struct {
    uint8_t width;
    const uint8_t *data;
} Letter;

static const uint8_t letter_A[] = {0x06, 0x09, 0x0F, 0x09, 0x09};
static const uint8_t letter_C[] = {0x07, 0x04, 0x04, 0x04, 0x07};
static const uint8_t letter_L[] = {0x04, 0x04, 0x04, 0x04, 0x07};
static const uint8_t letter_M[] = {0x0B, 0x1B, 0x0F, 0x0B, 0x0B};

// Add more letters as needed
static const Letter font[256] = {
    ['A'] = {5, letter_A},
    ['C'] = {5, letter_C},
    ['L'] = {5, letter_L},
    ['M'] = {5, letter_M},
    // Add other letters here
};

void TextRenderer__RenderString(view_frame_t *frame, const char *str) {
    // Clear the frame first
    memset(frame->red, 0, sizeof(frame->red));
    memset(frame->green, 0, sizeof(frame->green));
    memset(frame->blue, 0, sizeof(frame->blue));

    int len = strlen(str);
    int total_width = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 0 && c < 256 && font[c].data != NULL) {
            total_width += font[c].width + 1; // +1 for space between letters
        }
    }
    if (total_width > 0) total_width--; // remove last space

    int start_x = (16 - total_width) / 2; // center horizontally
    int y_offset = 16 - 5; // bottom align, since 5 high

    int x = start_x;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 0 && c < 256 && font[c].data != NULL) {
            const Letter *let = &font[c];
            for (int col = 0; col < let->width; col++) {
                if (x + col >= 16) break;
                uint8_t column_data = let->data[col];
                for (int row = 0; row < 5; row++) {
                    if (column_data & (1 << row)) {
                        // Set pixel at (x+col, y_offset + row)
                        int bit = (y_offset + row) * 16 + (x + col);
                        // Since frame is uint16_t[16], each uint16_t is a row
                        int row_idx = y_offset + row;
                        int col_idx = x + col;
                        if (row_idx < 16 && col_idx < 16) {
                            frame->red[row_idx] |= (1 << (15 - col_idx)); // MSB first?
                            // Assuming the display is MSB left, LSB right
                        }
                    }
                }
            }
            x += let->width + 1;
        }
    }
}