#ifndef SPI_H
#define SPI_H

#include "driver/spi_master.h"

#define SPI_HOST            SPI2_HOST
#define DMA_CHANNEL         SPI_DMA_CH_AUTO
#define SPI_CLOCK_SPEED_HZ  1 * 1000       // 10 kHz

//Bit Bang definitions
#define MS_DELAY_DATA   1
#define CLK_LO      0
#define CLK_HI      1
#define DATA_LO     0
#define DATA_HI     1

#define DEBUG 0

spi_device_handle_t Spi__Init(uint8_t, uint8_t, uint8_t, uint8_t);
void Spi__Write(spi_device_handle_t, uint8_t*, uint16_t);
// void Spi__Write_byte(spi_device_handle_t, uint8_t, uint8_t);

//bitbang
void Spi__BitBang_Setup(uint8_t, uint8_t);
void Spi__BitBang_Write(uint8_t, uint8_t, uint8_t*, uint16_t);

#endif