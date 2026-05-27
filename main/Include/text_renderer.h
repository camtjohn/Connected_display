#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "view.h"

// Function to render a string into a view_frame_t
// Assumes the frame is cleared, and renders at the bottom, centered horizontally
void TextRenderer__RenderString(view_frame_t *frame, const char *str);

#endif