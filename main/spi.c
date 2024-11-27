#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "spi.h"

const static char *TAG_SPI = "SPI";

esp_err_t ret;
spi_device_handle_t spi;

spi_device_handle_t Spi__Init(uint8_t data_pin, uint8_t clk_pin, uint8_t mode, uint8_t cs_pin) {
    spi_device_handle_t spi_handle;
    spi_bus_config_t buscfg = {                                         // Provide details to the SPI_bus_sturcture of pins and maximum data size
        //.miso_io_num = 0,
        .mosi_io_num = data_pin,
        .sclk_io_num = clk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // .max_transfer_sz = 512 * 8                                   // 4095 bytes is the max size of data that can be sent because of hardware limitations
        .max_transfer_sz = 0
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_CLOCK_SPEED_HZ,                           // Clock out at 12 MHz
        .mode = mode,                                                   // SPI mode 0: CPOL:-0 and CPHA:-0
        .spics_io_num = -1,                                         // This field is used to specify the GPIO pin that is to be used as CS'
        //.spics_io_num = cs_pin,                                         // This field is used to specify the GPIO pin that is to be used as CS'
        .queue_size = 1,                                                // We want to be able to queue 7 transactions at a time
    };

    ret = spi_bus_initialize(SPI_HOST, &buscfg, DMA_CHANNEL);           // Initialize the SPI bus
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(SPI_HOST, &devcfg, &spi_handle);           // Attach the Slave device to the SPI bus
    ESP_ERROR_CHECK(ret);

    return(spi_handle);
}

void Spi__Write(spi_device_handle_t spi_handle, uint16_t data, uint8_t len) {
    uint8_t data1 = data & 0xFF;
    uint8_t data2 = (data >> 8) & 0xFF;
    spi_transaction_t trans_desc = {
        // Configure the transaction_structure
        .flags = SPI_TRANS_USE_TXDATA,                                  // Set the Tx flag
        .tx_data = {data2, data1},                                      // The host will sent the address first followed by data we provided
        .length = len,                                                  // Length of the address + data is 16 bits
    };

    ret = spi_device_polling_transmit(spi_handle, &trans_desc);                // spi_device_polling_transmit starts to transmit entire 'trans_desc' structure.
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_SPI, "SPI operation failed\n");
    }
}

void Spi__BitBang_Setup(uint8_t clk_pin, uint8_t data_pin) {
    // gpio config clk and data pins
    gpio_reset_pin(clk_pin);
    gpio_reset_pin(data_pin);
    gpio_set_direction(clk_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(data_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(clk_pin, CLK_HI);
    gpio_set_level(data_pin, 0);
}

// Clock idles high. Pull clk low, wait, set data, wait, pull clock high (device reads on rising clk edge), wait
// cmd write = #-####-###X, len=9
// start_write = 101 AAAAAAA, len=10
// successive_write = DDDD, len=4
void Spi__BitBang_Write(uint8_t clk_pin, uint8_t data_pin, uint16_t data, uint8_t len) {
    // set MSB first. 
    for(uint8_t bit = len; bit > 0; bit--) {
        uint8_t shift_bit = bit-1;          //If len=9, shift bits 8 on first iteration. bit will decrement thereafter
        uint8_t data_value = (data >> shift_bit) & 1;
        if(DEBUG) {printf("%u", data_value);}
        gpio_set_level(clk_pin, CLK_LO);
        vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
        gpio_set_level(data_pin, data_value);
        vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
        gpio_set_level(clk_pin, CLK_HI);
        vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
    }
    if(DEBUG) {printf("\n");}
}