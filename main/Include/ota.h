#ifndef OTA_H
#define OTA_H

#include "freertos/semphr.h"

extern SemaphoreHandle_t startOTASemaphore;

// Initialize OTA system (validates boot state and creates download task)
void OTA__Init(void);

// Request boot to factory app for OTA update or reconfiguration
void OTA__Request_Factory_Boot(void);

void OTA__Trigger(void);

#endif