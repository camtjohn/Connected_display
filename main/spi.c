#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "spi.h"

const static char *TAG_SPI = "WEATHER_STATION: SPI";

esp_err_t ret;
spi_device_handle_t spi;

spi_device_handle_t Spi__Init(uint8_t data_pin, uint8_t clk_pin, uint8_t mode, uint8_t cs_pin) {
    spi_device_handle_t spi_handle;
    spi_bus_config_t buscfg = {                                         // Provide details to the SPI_bus_sturcture of pins and maximum data size
        .miso_io_num = -1,                                              // master in, slave out: data pin
        .mosi_io_num = data_pin,                                        // master out, slave in: data pin
        .sclk_io_num = clk_pin,                                         // clock pin
        .quadwp_io_num = -1,                                            // Write protect pin
        .quadhd_io_num = -1,                                            // Hold signal pin
        .max_transfer_sz = 0                                            // Max transfer size (bytes). If 0, defaults 4092 (DMA enabled) or SOC_SPI_MAXIMUM_BUFFER_SIZE (DMA disabled)
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_CLOCK_SPEED_HZ,                           // Clock out at 1 kHz
        .mode = mode,                                                   // SPI mode 0: CPOL:-0 and CPHA:-0
        .spics_io_num = cs_pin,                                         // This field is used to specify the GPIO pin that is to be used as CS'
        .queue_size = 1,                                                // We want to be able to queue 7 transactions at a time
    };

    // Lazy implementation for now: Only initialize bus once! Just for 8 (CS_RED) because it happens to be called first
    if(cs_pin==8) {
        ret = spi_bus_initialize(SPI_HOST, &buscfg, DMA_CHANNEL);           // Initialize the SPI bus
        ESP_ERROR_CHECK(ret);
    }

    ret = spi_bus_add_device(SPI_HOST, &devcfg, &spi_handle);           // Attach the Slave device to the SPI bus
    ESP_ERROR_CHECK(ret);

    return(spi_handle);
}

void Spi__Write(spi_device_handle_t spi_handle, uint8_t* buffer, uint16_t num_bits_data) {
    spi_transaction_t trans_desc;
    memset(&trans_desc, 0, sizeof(trans_desc));

    //flip bytes in buffer: first byte->last byte, 2nd byte->2nd to last byte...
    uint8_t num_bytes = num_bits_data / 8;
    uint8_t leftover_bits = num_bits_data % 8;
    if(leftover_bits > 0) {
        num_bytes ++;
    }
    uint8_t new_buffer[num_bytes];
    for(uint8_t i_byte = 0; i_byte < num_bytes; i_byte++) {
        uint8_t buf_idx = (num_bytes - 1) - i_byte;
        new_buffer[i_byte] = buffer[buf_idx];
    }
    
    // Configure the transaction_structure
    trans_desc.length = num_bits_data;                                   // Length of (address + data) is 16 bits
    trans_desc.tx_buffer = new_buffer;

    // Use queued transmit to block until the bus is free instead of erroring when a prior polling transaction is in-flight.
    ret = spi_device_transmit(spi_handle, &trans_desc);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_SPI, "SPI operation failed\n");
    }
}

// void Spi__Write_byte(spi_device_handle_t spi_handle, uint8_t data, uint8_t len) {
//     uint8_t data1 = data & 0xFF;
//     uint8_t data2 = (data >> 8) & 0xFF;
//     spi_transaction_t trans_desc = {
//         //  Configure the transaction_structure
//         .flags = SPI_TRANS_USE_TXDATA,                                  // Set the Tx flag
//         .tx_data = {data2, data1},                                      // The host will sent the address first followed by data we provided
//         .length = 8,                                                  // Length of (address + data) is 16 bits
//     };
//     ret = spi_device_polling_transmit(spi_handle, &trans_desc);                // spi_device_polling_transmit starts to transmit entire 'trans_desc' structure.
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG_SPI, "SPI operation failed\n");
//     }
// }

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
void Spi__BitBang_Write(uint8_t clk_pin, uint8_t data_pin, uint8_t* tx_buffer, uint16_t num_bits_data) {
    // set MSB first
    uint8_t num_bytes = num_bits_data / 8;
    if((num_bytes % 8) > 0) {
        num_bytes += 1;
    }

#if DEBUG
    printf("num_bits: %u  num_bytes: %u\n", num_bits_data, num_bytes);
    // uint16_t count_bits = 0;
#endif

    // Iterate over each byte in buffer MSB->LSB
    uint8_t curr_byte = num_bytes;  // 1-based: need last byte to be 1 for while loop to work properly
    while(curr_byte > 0) {
        uint8_t data_byte = tx_buffer[curr_byte-1];

        // Set data according to each bit in byte MSB->LSB
        for(uint8_t bit = 8; bit > 0; bit--) {
            uint8_t bit_index = bit - 1;
            uint8_t level = ((data_byte >> bit_index) & 1);
            
            gpio_set_level(clk_pin, CLK_LO);
            vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
            gpio_set_level(data_pin, level);
            vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
            gpio_set_level(clk_pin, CLK_HI);
            vTaskDelay(pdMS_TO_TICKS(MS_DELAY_DATA));
        #if DEBUG
            printf("%u", level);
            //     count_bits ++;
            //     if(count_bits >= num_bits_data) {break;}
        #endif
        }
#if DEBUG 
        printf(":");
#endif
        curr_byte --;
    }

#if DEBUG
    printf("\n");
#endif
}