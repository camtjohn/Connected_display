#ifndef ETCHSKETCH_H
#define ETCHSKETCH_H

#include "view.h"
#include "mqtt_protocol.h"

//PUBLIC FUNCTION
void Etchsketch__Initialize(void);
void Etchsketch__On_Enter(void);
void Etchsketch__Get_view(view_frame_t*);

void Etchsketch__UI_Encoder_Top(uint8_t);
void Etchsketch__UI_Encoder_Side(uint8_t);
void Etchsketch__UI_Button(uint8_t);
void Etchsketch__UI_Button_Released(uint8_t);

// Shared view inline API
void Etchsketch__Request_full_sync(void);
void Etchsketch__Apply_remote_frame(const mqtt_shared_view_frame_t *frame);

#endif