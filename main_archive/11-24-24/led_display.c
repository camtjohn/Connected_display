#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "led_display.h"
#include "spi.h"


// PRIVATE method prototypes
// Views
int bit_arr_to_int(int*);

// GPIO config
void config_all_gpio(void);
void config_gpio_output(int);
void write_spi_clear(void);
void write_row_to_spi(uint8_t, uint16_t);
void write_row_gpio(uint16_t);
void set_select_pins(uint8_t);
void toggle_gpio(uint8_t);


// PUBLIC methods

void Led_display__Initialize(void) {
    config_all_gpio();

    #if USE_SPI
    Spi__Init();
    printf("Using SPI\n");
    #else
    printf("Not using SPI\n");
    #endif
}

void Led_display__Display_view(uint16_t *row_arr) {
    gpio_set_level(OE_PIN, 0);  //Enable output
        for(uint8_t i_row=0; i_row<16; i_row++) {
            #if USE_SPI
            // Clear row before setting Leds. Tried enabling/disabling OE pin but this works better
            write_spi_clear();
            write_row_to_spi(i_row, row_arr[i_row]);
            #else
            set_select_pins(i_row);        //Enable rows select pins
            write_row_gpio(~((uint16_t) i_row));           //Send row led states, toggle set pin
            #endif
            vTaskDelay(pdMS_TO_TICKS(9));
        }
        // DISable output: Prevent last row from being brighter due to enabled longer
        gpio_set_level(OE_PIN, 1);
}


// PRIVATE METHODS

void config_all_gpio(void) {
    int select_pins[4];
    #if (!USE_SPI)
    config_gpio_output(DATA_PIN);
    config_gpio_output(CLK_PIN);
    #endif
    config_gpio_output(SET_PIN);
    config_gpio_output(OE_PIN);

    select_pins[0] = SELECT1_PIN;
    select_pins[1] = SELECT2_PIN;
    select_pins[2] = SELECT3_PIN;
    select_pins[3] = SELECT4_PIN;
    for(int i_select=0; i_select<4; i_select++) {
        config_gpio_output(select_pins[i_select]);
    }
}

void config_gpio_output(int pin_number) {
    gpio_reset_pin(pin_number);
    gpio_set_direction(pin_number, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_number, 0);
}

void write_spi_clear(void) {
    Spi__Write(0xFFFF);
    toggle_gpio(SET_PIN);
}

void write_row_to_spi(uint8_t row, uint16_t led_states) {
    set_select_pins(row);
    Spi__Write(~led_states);
    toggle_gpio(SET_PIN);
}

// Set i_LED based on i_bit of integer "led_row"
// ex: int row = 5 = 0000 0101 -> set 1st led and 3rd led
void write_row_gpio(uint16_t temp_led_row_states) {
    uint16_t mask_leds = 0x8000;    // 16th bit set
    for(uint8_t i=0; i<16; i++) {
        // Set LED according to i_bit of "led_row"
        gpio_set_level(DATA_PIN, (temp_led_row_states & mask_leds));
        mask_leds >>= 1;
        toggle_gpio(CLK_PIN);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    toggle_gpio(SET_PIN);
}

void toggle_gpio(uint8_t pin_number) {
    gpio_set_level(pin_number, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(pin_number, 0);
}

// Set the 4 select lines according to the first 4 bit states of "select_row"
void set_select_pins(uint8_t select_row) {
    gpio_set_level(SELECT1_PIN, select_row & 1);    // LSB=bit1 of input
    gpio_set_level(SELECT2_PIN, select_row & 2);    // bit2
    gpio_set_level(SELECT3_PIN, select_row & 4);    // bit3
    gpio_set_level(SELECT4_PIN, select_row & 8);    // bit4
}