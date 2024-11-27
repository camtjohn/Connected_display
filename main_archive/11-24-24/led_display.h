#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H

// To set row LEDs according to bits, either use SPI or bit bang gpio
#define USE_SPI     1

#define SET_PIN         10
#define OE_PIN          18

#define SELECT1_PIN     4
#define SELECT2_PIN     5
#define SELECT3_PIN     6
#define SELECT4_PIN     7

// Set the 4 select lines according to the first 4 bit states of "select_row"
void Led_display__Initialize(void);
void Led_display__Display_view(uint16_t*);

#endif