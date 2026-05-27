#ifndef MUSIC_H
#define MUSIC_H

#include "view.h"

//PUBLIC FUNCTION
void Music__Initialize(void);
void Music__Get_view(view_frame_t *frame);

void Music__UI_Encoder_Side(uint8_t);
void Music__UI_Encoder_Top(uint8_t);
void Music__UI_Button(uint8_t);

#endif