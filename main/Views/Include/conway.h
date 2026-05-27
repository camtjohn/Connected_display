#ifndef CONWAY_H
#define CONWAY_H

#include "view.h"

//PUBLIC FUNCTION
void Conway__Initialize(void);
void Conway__Get_frame(view_frame_t *frame);
uint32_t Conway__Get_refresh_rate_ms(void);

void Conway__UI_Encoder_Top(uint8_t);
void Conway__UI_Encoder_Side(uint8_t);
void Conway__UI_Button(uint8_t);

#endif