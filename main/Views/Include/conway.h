#ifndef CONWAY_H
#define CONWAY_H

#include "view.h"

//PUBLIC FUNCTION
void Conway__Initialize(void);
uint16_t Conway__Get_frame(view_frame_t *frame);

void Conway__UI_Encoder_Top(uint8_t);
void Conway__UI_Encoder_Side(uint8_t);
void Conway__UI_Button(uint8_t);

#endif