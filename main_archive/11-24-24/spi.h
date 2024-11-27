#ifndef SPI_H
#define SPI_H

#define DATA_PIN        19
#define CLK_PIN         1

#define SPI_HOST            SPI2_HOST
#define DMA_CHANNEL         SPI_DMA_CH_AUTO
#define SPI_CLOCK_SPEED_HZ  10 * 1000       // 10 kHz
#define SPI_MODE            0                       // SPI mode 0: CPOL 0=clock idle low, CPHA 0=sample on rise/fall? edge
// #define SPI_SPI_CS_PIN      15              // Example GPIO pin for CS

void Spi__Init(void);
void Spi__Write(uint16_t);

#endif

