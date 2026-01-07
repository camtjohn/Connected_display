#ifndef OTA_H
#define OTA_H

#include "freertos/semphr.h"

extern SemaphoreHandle_t startOTASemaphore;

void OTA__Init(void);
void OTA__Trigger(void);

#endif