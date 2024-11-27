#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H

// To set row LEDs according to bits, either use SPI or bit bang gpio
#define USE_SPI_BIT_BANG     1

// define pins for spi protocol
#define CS_BLUE         7
#define CS_RED          8
#define DATA_PIN        9
#define CLK_PIN         10 

#define SPI_MODE        3   // Clock idle high, sample on rising edge
#define CS_ACTIVE       0
#define CS_INACTIVE     1

// commands bootup led driver
#define CMD_MODE        4       // 100
#define CMD_LEN         9       // CCCC-CCCC-X
#define SET_WRITE_MODE  0x0280  // 0000-0010-1000-0000: 101 AAAAAAA
#define SET_WRITE_LEN   10      // 101 AAAA AAA
#define WRITE_LEN       4       // DDDD

// following definitions are the 8 most sig bits of the config
// will need to shift these left one to account for extra bit at LSB
#define SYS_DIS         0       // 0000 0000  -disable sys osc,LED duty
#define COM_OPTION      0x24    // 0010 01XX  -NMOS 16 COM
#define SET_SLAVE       0x10    // 0001 0XXX  -OSC,SYNC input
#define SET_MASTER      0x18    // 0001 10XX  -OSC,SYNC output
#define SYS_ON          0x01    // 0000 0001  -enable system osc
#define LED_ON          0x03    // 0000 0011  -enable LED duty
#define LED_OFF         0x02    // 0000 0010  -disable LED duty
#define PWM_DUTY        0xA1    // 101X DDDD  - set D to desired duty


void Led_driver__Initialize(void);
void Led_driver__Setup(void);
void Led_driver__Update_RAM(uint16_t*, uint16_t*, uint16_t*);
void Led_driver__Set_brightness(uint8_t);

#endif