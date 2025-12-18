#ifndef UI_H
#define UI_H

#define PRESSED     1
#define NOT_PRESSED 0

// Button debounce time (milliseconds)
#define UI_BUTTON_DEBOUNCE_MS (15)

// Refractory period after button event to prevent jitter (milliseconds)
#define UI_BUTTON_REFRACTORY_MS (50)

#define UI_ENCODER_TASK_PERIOD_MS (15)

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

// Check if a specific button is currently pressed (for paint mode, etc.)
// button_num: 1-4 (returns 1 if pressed, 0 if not)
uint8_t Ui__Is_Button_Pressed(uint8_t button_num);

#endif