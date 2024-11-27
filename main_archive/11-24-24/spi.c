#include "driver/spi_master.h"
#include "esp_log.h"
#include "spi.h"

const static char *TAG_SPI = "SPI";

esp_err_t ret;
spi_device_handle_t spi;

void Spi__Init(void) {
    spi_bus_config_t buscfg = {                                         // Provide details to the SPI_bus_sturcture of pins and maximum data size
        //.miso_io_num = 0,
        .mosi_io_num = DATA_PIN,
        .sclk_io_num = CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // .max_transfer_sz = 512 * 8                                   // 4095 bytes is the max size of data that can be sent because of hardware limitations
        .max_transfer_sz = 0
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_CLOCK_SPEED_HZ,                           // Clock out at 12 MHz
        .mode = SPI_MODE,                                               // SPI mode 0: CPOL:-0 and CPHA:-0
        .spics_io_num = -1,                                             // This field is used to specify the GPIO pin that is to be used as CS'
        .queue_size = 1,                                                // We want to be able to queue 7 transactions at a time
    };

    ret = spi_bus_initialize(SPI_HOST, &buscfg, DMA_CHANNEL);       // Initialize the SPI bus
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(SPI_HOST, &devcfg, &spi);                  // Attach the Slave device to the SPI bus
    ESP_ERROR_CHECK(ret);
}

void Spi__Write(uint16_t data) {
    uint8_t data1 = data & 0xFF;
    uint8_t data2 = (data >> 8) & 0xFF;
    spi_transaction_t trans_desc = {
        // Configure the transaction_structure
        .flags = SPI_TRANS_USE_TXDATA,                                  // Set the Tx flag
        .tx_data = {data2, data1},                                      // The host will sent the address first followed by data we provided
        .length = 16,                                                   // Length of the address + data is 16 bits
    };

    ret = spi_device_polling_transmit(spi, &trans_desc);                // spi_device_polling_transmit starts to transmit entire 'trans_desc' structure.
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_SPI, "SPI operation failed\n");
    }
}