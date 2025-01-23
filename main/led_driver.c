#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "led_driver.h"
#include "spi.h"

// PRIVATE variables
static uint16_t RAM_red[24];
static uint16_t RAM_blue[24];

// PRIVATE method prototypes
void configure_GPIO_output(uint8_t);
void send_command_msg(uint8_t);
void start_multiple_msg(uint8_t, uint16_t, uint8_t);
void set_cs(uint8_t, uint8_t);
void send_successive_RAM_msg(uint8_t);
void clear_RAM(uint8_t);
void write_RAM(uint16_t);
void update_RAM(uint8_t, uint16_t*);
void translate_views_to_RAM_red(uint16_t *, uint16_t *, uint16_t *);
void translate_views_to_RAM_blue(uint16_t *, uint16_t *, uint16_t *, uint16_t *);


// PUBLIC methods

void Led_driver__Initialize(void) {
    #if USE_SPI_BIT_BANG
    printf("Using SPI\n");
    Spi__BitBang_Setup(CLK_PIN, DATA_PIN);

    // select pin. idle high
    configure_GPIO_output(CS_RED);
    configure_GPIO_output(CS_BLUE);

    #else
    printf("Not using SPI\n");
    Spi__Init();
    #endif
}

// Send commands to Led drivers at bootup
void Led_driver__Setup(void) {
    start_multiple_msg(CS_RED, CMD_MODE, 3);
    send_command_msg(SYS_DIS);
    send_command_msg(COM_OPTION);
    send_command_msg(SET_MASTER);
    send_command_msg(SYS_ON);
    send_command_msg(PWM_DUTY);
    send_command_msg(LED_ON);
    set_cs(CS_RED, CS_INACTIVE);

    start_multiple_msg(CS_BLUE, CMD_MODE, 3);
    send_command_msg(SYS_DIS);
    send_command_msg(COM_OPTION);
    send_command_msg(SET_SLAVE);
    send_command_msg(SYS_ON);
    send_command_msg(PWM_DUTY);
    send_command_msg(LED_ON);
    set_cs(CS_BLUE, CS_INACTIVE);
}

void Led_driver__Update_RAM(uint16_t *view_red, uint16_t *view_green, uint16_t *view_blue) {
    //translate view to RAM
    translate_views_to_RAM_red(view_red, view_green, RAM_red);
    translate_views_to_RAM_blue(view_red, view_green, view_blue, RAM_blue);

    update_RAM(CS_RED, RAM_red);
    update_RAM(CS_BLUE, RAM_blue);
}

// Brightness 0 (on but most dim) to 15 (brightest)
void Led_driver__Set_brightness(uint8_t level) {
    level &= 0xF;
    uint8_t pwm_duty = 0xA0 | level;    // 101X DDDD

    start_multiple_msg(CS_RED, CMD_MODE, 3);
    send_command_msg(pwm_duty);
    set_cs(CS_RED, CS_INACTIVE);

    start_multiple_msg(CS_BLUE, CMD_MODE, 3);
    send_command_msg(pwm_duty);
    set_cs(CS_BLUE, CS_INACTIVE);
}

// Turn display on/off
void Led_driver__Toggle_LED(uint8_t led_state) {
    uint8_t cmd;
    if(led_state) {
        cmd = LED_ON;
    } else {
        cmd = LED_OFF;
    }
    start_multiple_msg(CS_RED, CMD_MODE, 3);
    send_command_msg(cmd);
    set_cs(CS_RED, CS_INACTIVE);

    start_multiple_msg(CS_BLUE, CMD_MODE, 3);
    send_command_msg(cmd);
    set_cs(CS_BLUE, CS_INACTIVE);
}


// PRIVATE METHODS

void configure_GPIO_output(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    set_cs(pin, CS_INACTIVE);
}

void start_multiple_msg(uint8_t dev, uint16_t mode, uint8_t len) {
    set_cs(dev, CS_ACTIVE);
    #if USE_SPI_BIT_BANG
    Spi__BitBang_Write(CLK_PIN, DATA_PIN, mode, len);

    #else
    spi_device_handle_t spi_handle;
    Spi__Write(spi_handle, mode, len);
    #endif
}

void send_command_msg(uint8_t cmd) {
    uint16_t payload = cmd << 1;    // 00##-####-X (cmd variable just includes MSB 8 bits)
    #if USE_SPI_BIT_BANG
    Spi__BitBang_Write(CLK_PIN, DATA_PIN, payload, CMD_LEN);

    #else
    spi_device_handle_t spi_handle;
    Spi__Write(spi_handle, payload, CMD_LEN);
    #endif
}

// Allow time for settle, Set CS pin, Allow time for settle
void set_cs(uint8_t pin, uint8_t level) {
    if(level) {
        gpio_set_level(pin, 1);
    } else {
        gpio_set_level(pin, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
}

// void send_RAM_msg_with_Addr(uint8_t addr, uint8_t data) {
//     addr &= 0x7F;                   // mask first 7 bits
//     addr <<= 4;
//     uint16_t payload = addr;        // 0010 1AAA AAAA 0000
//     data &= 0xF;                    // mask first 4 bits
//     payload |= data;                // 0000 0AAA AAAA DDDD
//     Spi__BitBang_Write(CLK_PIN, DATA_PIN, payload, 11);
// }

void send_successive_RAM_msg(uint8_t data) {
    // mask first 4 bits
    data &= 0xF;
    
    #if USE_SPI_BIT_BANG
    Spi__BitBang_Write(CLK_PIN, DATA_PIN, (uint16_t)data, 4);

    #else
    spi_device_handle_t spi_handle;
    Spi__Write(spi_handle, (uint16_t)data, 4);
    #endif
}

void clear_RAM(uint8_t dev) {
    start_multiple_msg(dev, SET_WRITE_MODE, SET_WRITE_LEN);
    // Each of the 24 rows made up of 4 addresses. Each address holds 4 bits
    uint8_t num_addrs = 0x60;    // (24 rows)*(4 addr per row) = 96 addresses
    // clear ram
    uint8_t data = 0;
    for(uint8_t addr_index = 0; addr_index < num_addrs; addr_index++) {
        send_successive_RAM_msg(data);
        vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
    }
    set_cs(dev, CS_INACTIVE);
}

// Data registered on a rising edge of the clk signal
void write_RAM(uint16_t RAM_row) {
    // 4 addresses per row
    for(uint8_t addr = 4; addr > 0; addr--) {
        uint8_t data = (RAM_row >> ((addr-1)*4)) & 0xF;     // eg. First iteration: ####-xxxx-xxxx-xxxx. shift 12 right, mask 4 bits
        send_successive_RAM_msg(data);
        vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
    }
}

void update_RAM(uint8_t dev, uint16_t *RAM_mem) {
    start_multiple_msg(dev, SET_WRITE_MODE, SET_WRITE_LEN);
    for(uint8_t row=0; row<24; row++) {
        write_RAM(RAM_mem[row]);
    }
    set_cs(dev, CS_INACTIVE);
}

// view[0]->col 16, view[15]->col 1
// view index = 16-col
void translate_views_to_RAM_red(uint16_t *view_red, uint16_t *view_green, uint16_t *ret_RAM_red) {
    ret_RAM_red[0] = view_green[2];     //col14
    ret_RAM_red[1] = view_red[3];
    ret_RAM_red[2] = view_green[3];
    ret_RAM_red[3] = view_green[4];
    ret_RAM_red[4] = view_red[5];
    ret_RAM_red[5] = view_green[1];
    ret_RAM_red[6] = view_red[1];
    ret_RAM_red[7] = view_red[0];
    ret_RAM_red[8] = view_green[5];
    ret_RAM_red[9] = view_red[6];
    ret_RAM_red[10] = view_green[6];
    ret_RAM_red[11] = view_green[7];
    ret_RAM_red[12] = view_red[8];
    ret_RAM_red[13] = view_red[9];
    ret_RAM_red[14] = view_green[9];
    ret_RAM_red[15] = view_red[10];
    ret_RAM_red[16] = view_red[11];
    ret_RAM_red[17] = view_green[11];
    ret_RAM_red[18] = view_red[12];
    ret_RAM_red[19] = view_green[12];
    ret_RAM_red[20] = view_green[13];
    ret_RAM_red[21] = view_green[15];
    ret_RAM_red[22] = view_green[14];
    ret_RAM_red[23] = view_red[15];
}

void translate_views_to_RAM_blue(uint16_t *view_red, uint16_t *view_green, uint16_t *view_blue, uint16_t *ret_RAM_blue) {
    ret_RAM_blue[0] = view_green[10];
    ret_RAM_blue[1] = view_blue[11];
    ret_RAM_blue[2] = view_blue[12];
    ret_RAM_blue[3] = view_red[13];
    ret_RAM_blue[4] = view_blue[13];
    ret_RAM_blue[5] = view_red[14];
    ret_RAM_blue[6] = view_blue[14];
    ret_RAM_blue[7] = view_blue[15];
    ret_RAM_blue[8] = view_blue[10];
    ret_RAM_blue[9] = view_blue[9];
    ret_RAM_blue[10] = view_green[8];
    ret_RAM_blue[11] = view_blue[8];
    ret_RAM_blue[12] = view_blue[7];
    ret_RAM_blue[13] = view_red[7];
    ret_RAM_blue[14] = view_blue[6];
    ret_RAM_blue[15] = view_blue[5];
    ret_RAM_blue[16] = view_red[4];
    ret_RAM_blue[17] = view_blue[4];
    ret_RAM_blue[18] = view_blue[3];
    ret_RAM_blue[19] = view_red[2];
    ret_RAM_blue[20] = view_blue[2];
    ret_RAM_blue[21] = view_green[0];
    ret_RAM_blue[22] = view_blue[1];
    ret_RAM_blue[23] = view_blue[0];
}