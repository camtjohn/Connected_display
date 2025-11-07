#ifndef ETCHSKETCH_H
#define ETCHSKETCH_H

//PUBLIC FUNCTION
void Etchsketch__Initialize(void);
void Etchsketch__Get_view(uint16_t*, uint16_t*, uint16_t*);

void Etchsketch__UI_Encoder_Top(uint8_t);
void Etchsketch__UI_Encoder_Side(uint8_t);
void Etchsketch__UI_Button(uint8_t);

#endif