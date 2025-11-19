#ifndef OTA_H
#define OTA_H

#include "freertos/semphr.h"

#define FW_VERSION 2

extern SemaphoreHandle_t startOTASemaphore;

void OTA__Init(void);

#endif