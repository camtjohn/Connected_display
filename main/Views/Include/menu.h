#ifndef MENU_H
#define MENU_H

#include "view.h"

// Menu-selectable views (subset of View_type, excludes VIEW_PROVISIONING)
typedef enum {
    MENU_VIEW_WEATHER = 0,
    MENU_VIEW_CONWAY,
    MENU_VIEW_ETCHSKETCH,
    NUM_MENU_VIEWS
} Menu_view_type;

//PUBLIC FUNCTION
void Menu__Initialize(void);
void Menu__Get_view(view_frame_t *frame);
View_type Menu__Get_current_view(void);

void Menu__UI_Encoder_Top(uint8_t);
void Menu__UI_Encoder_Side(uint8_t);
void Menu__UI_Button(uint8_t);

#endif