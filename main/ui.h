#ifndef UI_H
#define UI_H

#define PRESSED     1
#define NOT_PRESSED 0

// GPIO Pin Definitions
#define BTN_1       13
#define BTN_2       12
#define BTN_3       14
#define BTN_4       15
#define ENC_1_A     2
#define ENC_1_B     1
#define ENC_2_A     3
#define ENC_2_B     4

void Ui__Initialize(void);
void Ui__Monitor_poll_btns(void);
void Ui__Monitor_poll_encoders(void);

#endif