#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "led_driver.h"
#include "spi.h"

// const static char *TAG = "WEATHER_STATION: LED_DRIVER";

// PRIVATE variables
static uint16_t RAM_red[24];
static uint16_t RAM_blue[24];

spi_device_handle_t Spi_Handle_Red;
spi_device_handle_t Spi_Handle_Blue;

// PRIVATE method prototypes
void configure_GPIO_output(uint8_t);
void set_cs(uint8_t, uint8_t);
uint16_t add_payload_to_buf(uint8_t*, uint16_t, uint16_t, uint8_t);
void send_setup_messages(uint8_t);
void send_single_command(uint8_t, uint8_t);
void update_RAM(uint8_t, uint16_t*);
void clear_RAM(uint8_t);
void transmit_spi(uint8_t, uint8_t*, uint16_t);

uint16_t insert_bits_MSB_buffer(uint8_t*, uint16_t, uint8_t, uint8_t);

void translate_views_to_RAM_red(uint16_t *, uint16_t *, uint16_t *);
void translate_views_to_RAM_blue(uint16_t *, uint16_t *, uint16_t *, uint16_t *);


// PUBLIC methods

void Led_driver__Initialize(void) {
    #if USE_SPI_BIT_BANG
    printf("Using SPI (bitbang)\n");
    configure_GPIO_output(CS_RED);
    configure_GPIO_output(CS_BLUE);
    Spi__BitBang_Setup(CLK_PIN, DATA_PIN);

    #else
    printf("Using SPI (actual peripheral):\n");
    Spi_Handle_Red = Spi__Init(DATA_PIN, CLK_PIN, SPI_MODE, CS_RED);    // Unused pin 46 used for CS til incorporate CS into SPI
    Spi_Handle_Blue = Spi__Init(DATA_PIN, CLK_PIN, SPI_MODE, CS_BLUE);    // Unused pin 46 used for CS til incorporate CS into SPI
    #endif
}

// Send commands to Led drivers at bootup
void Led_driver__Setup(void) {
    send_setup_messages(CS_RED);
    send_setup_messages(CS_BLUE);
}

void Led_driver__Update_RAM(uint16_t *view_red, uint16_t *view_green, uint16_t *view_blue) {
    //translate view to RAM
    translate_views_to_RAM_red(view_red, view_green, RAM_red);
    translate_views_to_RAM_blue(view_red, view_green, view_blue, RAM_blue);

    update_RAM(CS_RED, RAM_red);
    update_RAM(CS_BLUE, RAM_blue);
}

void Led_driver__Clear_RAM(void) {
    clear_RAM(CS_RED);
    clear_RAM(CS_BLUE);
}

// Brightness 0 (on but most dim) to 15 (brightest)
void Led_driver__Set_brightness(uint8_t level) {
    level &= 0xF;
    uint8_t pwm_duty = 0xA0 | level;    // 101X DDDD

    send_single_command(pwm_duty, CS_RED);
    send_single_command(pwm_duty, CS_BLUE);
}

// Turn display on/off
void Led_driver__Toggle_LED(uint8_t led_state) {
    uint8_t cmd;
    if(led_state==1) {
        cmd = LED_ON;
    } else {
        cmd = LED_OFF;
    }

    send_single_command(cmd, CS_RED);
    send_single_command(cmd, CS_BLUE);
}


// PRIVATE METHODS

void configure_GPIO_output(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    set_cs(pin, CS_INACTIVE);
}

// Allow time for settle, Set CS pin, Allow time for settle
void set_cs(uint8_t pin, uint8_t level) {
    vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
    if(level) {
        gpio_set_level(pin, 1);
    } else {
        gpio_set_level(pin, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
}

uint16_t add_payload_to_buf(uint8_t * tx_buffer, uint16_t curr_idx, uint16_t payload, uint8_t num_bits_payload) {
    uint8_t byte1 = (payload >> 8) & 0xFF ;
    uint8_t byte2 = payload & 0xFF;

    // Spi__BitBang_Write(CLK_PIN, DATA_PIN, payload, CMD_LEN);
    if(num_bits_payload <= 8) {
        curr_idx = insert_bits_MSB_buffer(tx_buffer, curr_idx, byte2, num_bits_payload);
    } else {
        curr_idx = insert_bits_MSB_buffer(tx_buffer, curr_idx, byte1, (num_bits_payload - 8));
        curr_idx = insert_bits_MSB_buffer(tx_buffer, curr_idx, byte2, 8);
    }

    return curr_idx;
}

void send_setup_messages(uint8_t dev) {
    // Number of data bits in buffer = 3 + (9 * 6) = 57. Round up to nearest 8bits = 64 = 8bytes
    uint8_t tx_buffer[8];
    uint16_t current_idx;
    uint16_t num_bits_buffer;
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    uint8_t len_buffer_bits = 64;
    current_idx = len_buffer_bits - 1;
    
    // Assemble buffer
    current_idx = add_payload_to_buf(tx_buffer, current_idx, CMD_MODE, 3);
    num_bits_buffer = 3;    // CMD_MODE is 3 bits

    current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(SYS_DIS << 1)), 9);
    current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(COM_OPTION << 1)), 9);
    if(dev == CS_RED) {
        current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(SET_MASTER << 1)), 9);
    } else {
        current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(SET_SLAVE << 1)), 9);
    }
    current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(SYS_ON << 1)), 9);
    current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(PWM_DUTY << 1)), 9);
    current_idx = add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(LED_ON << 1)), 9);
    num_bits_buffer += (6 * 9);

    transmit_spi(dev, tx_buffer, num_bits_buffer);
}

void send_single_command(uint8_t cmd, uint8_t dev) {    
    // Number of data bits in buffer = 3 + 9 = 12. Round up nearest 8 bits = 16 = 2bytes
    uint8_t tx_buffer[2];
    uint16_t current_idx = 0;
    uint16_t num_bits_buffer = 0;
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    uint8_t len_buffer_bits = 8 * 2;
    current_idx = len_buffer_bits - 1;
    
    // Assemble buffer
    current_idx = add_payload_to_buf(tx_buffer, current_idx, CMD_MODE, 3);
    num_bits_buffer += 3;

    add_payload_to_buf(tx_buffer, current_idx, ((uint16_t)(cmd << 1)), 9);
    num_bits_buffer += 9;

    transmit_spi(dev, tx_buffer, num_bits_buffer);
}

void update_RAM(uint8_t dev, uint16_t *RAM_mem) {
    // Total num bits = ( 24 rows * 4 addr/row  * 4 bits/addr) + 10 mode bits = 394 bits
    // Round up nearest 8 bits = 400 = 50bytes
    uint8_t tx_buffer[50];
    uint16_t current_idx;
    memset(&tx_buffer, 0, sizeof(tx_buffer));

    uint16_t len_buffer_bits = 50 * 8;        // Round up nearest 8 bits = 400 (50 bytes)
    current_idx = len_buffer_bits - 1;  // MSBit of buffer

    // // Asemble buffer
    // // Insert mode bits
    current_idx = add_payload_to_buf(tx_buffer, current_idx, SET_WRITE_MODE, SET_WRITE_LEN);
    // // Insert data bits at appropriate place in buffer
    // // eg. First iteration: ####-xxxx-xxxx-xxxx. shift 12 right, mask 4 bits
    for(uint8_t row=0; row<24; row++) {
        uint16_t data_row = RAM_mem[row];
        for(uint8_t addr = 4; addr > 0; addr--) {
            uint8_t shifted_data = data_row >> ((addr - 1) * 4);
            uint8_t masked_data = shifted_data & 0xF;
            current_idx = insert_bits_MSB_buffer(tx_buffer, current_idx, masked_data, 4);
        }
    }
    uint16_t num_bits_buffer = 394;
    
    transmit_spi(dev, tx_buffer, num_bits_buffer);
}

void clear_RAM(uint8_t dev) {
    // total num bits = ( 24 rows * 4 addr/row  * 4 bits/addr) + 10 mode bits = 394 bits
    // round up nearest 8 bits = 400 bits = 50bytes
    uint8_t tx_buffer[50];
    uint16_t current_idx;
    uint16_t num_bits_buffer;

    memset(&tx_buffer, 0, sizeof(tx_buffer));

    uint16_t len_buffer_bits = 50 * 8;        // Round up nearest 8 bits = 400 (50 bytes)
    current_idx = len_buffer_bits - 1;  // MSBit of buffer

    // Asemble buffer
    add_payload_to_buf(tx_buffer, current_idx, SET_WRITE_MODE, SET_WRITE_LEN);
    num_bits_buffer = 394;

    transmit_spi(dev, tx_buffer, num_bits_buffer);
}

void transmit_spi(uint8_t dev, uint8_t * tx_buffer, uint16_t num_bits_buffer) {
    #if USE_SPI_BIT_BANG
        set_cs(dev, CS_ACTIVE);
        Spi__BitBang_Write(CLK_PIN, DATA_PIN, tx_buffer, num_bits_buffer);
        set_cs(dev, CS_INACTIVE);
    #else
        if(dev==CS_RED) {
            Spi__Write(Spi_Handle_Red, tx_buffer, num_bits_buffer);
        } else {
            Spi__Write(Spi_Handle_Blue, tx_buffer, num_bits_buffer);
        }
    #endif
}

// Insert data bits into array buffer
uint16_t insert_bits_MSB_buffer(uint8_t * tx_buffer, uint16_t current_idx, uint8_t data, uint8_t num_bits_data) {
    // ex scenario: buf[ ] = [0110 01XX] [xxxx xxxx], idx=9, data={101}
    //              shift data >> 1, insert into 1st byte:
    //              buf[2] = [0110 0110]
    //              shift data << 5 = (8-num_bits_data)
    //              buf[1] = [01xx xxxx]

    // indexes zero based! [7 6 5 4  3 2 1 0]
    uint8_t current_byte = current_idx / 8;
    uint8_t curr_idx_in_byte = current_idx % 8;
    uint8_t empty_bits_byte1 = curr_idx_in_byte + 1;

    if(num_bits_data < empty_bits_byte1) {  // data can fit fully into current byte
        // shift data left if necessary
        data <<= (empty_bits_byte1 - num_bits_data);
        tx_buffer[current_byte] |= data;
    
    } else {    // data split between 2 bytes
        uint8_t shifted_data_first_byte = data >> (num_bits_data - empty_bits_byte1);
        tx_buffer[current_byte] |= shifted_data_first_byte;

        uint8_t bits_left_in_data = num_bits_data - empty_bits_byte1;
        uint8_t shifted_data_second_byte = data << (8 - bits_left_in_data);
        tx_buffer[current_byte - 1] |= shifted_data_second_byte;
    }

    return (current_idx - num_bits_data);
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