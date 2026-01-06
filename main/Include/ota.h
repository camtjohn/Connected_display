#ifndef OTA_H
#define OTA_H

// Initialize OTA system (validates boot state)
void OTA__Init(void);

// Request boot to factory app for OTA update or reconfiguration
void OTA__Request_Factory_Boot(void);

#endif