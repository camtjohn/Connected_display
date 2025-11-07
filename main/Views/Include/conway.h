#ifndef CONWAY_H
#define CONWAY_H

//PUBLIC FUNCTION
void Conway__Initialize(void);
uint16_t Conway__Get_frame(uint16_t *, uint16_t *, uint16_t *);

void Conway__UI_Encoder_Top(uint8_t);
void Conway__UI_Encoder_Side(uint8_t);
void Conway__UI_Button(uint8_t);

#endif