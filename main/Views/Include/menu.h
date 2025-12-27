#ifndef MENU_H
#define MENU_H

#include "view.h"

//PUBLIC FUNCTION
void Menu__Initialize(void);
void Menu__Get_view(view_frame_t *frame);
View_type Menu__Get_current_view(void);

void Menu__UI_Encoder_Top(uint8_t);
void Menu__UI_Encoder_Side(uint8_t);
void Menu__UI_Button(uint8_t);

#endif