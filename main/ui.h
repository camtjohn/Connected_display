#ifndef UI_H
#define UI_H

#define PRESSED     1
#define NOT_PRESSED 0

// UI
#define BTN_1       13
#define BTN_2       12
#define BTN_3       14
#define BTN_4       15

void Ui__Initialize(void);
uint8_t Ui__Monitor_poll_btns(void);
void Ui__Btn_action(uint8_t);


#endif