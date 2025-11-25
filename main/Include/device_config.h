#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"


#define DEFAULT_DEVICE_NUMBER   "dev001"
#define DEFAULT_ZIP_CODE        "60607"
#define DEFAULT_BRIGHT          1
#define DEFAULT_TIMEZONE        -4


// Device configuration structure
typedef struct {
    char device_name[16];     // "device002"
    char zip_code[16];        // "12345" or "12345-6789"
    uint8_t brightness;       // Optional: persistent brightness
    int8_t timezone_offset;   // Optional: UTC offset (can be negative)
} device_config_t;

// Public API
esp_err_t DeviceConfig_Init(void);
esp_err_t DeviceConfig_Load(device_config_t *config);
esp_err_t DeviceConfig_Save(const device_config_t *config);
esp_err_t DeviceConfig_GetDeviceName(char *buffer, size_t buffer_size);
esp_err_t DeviceConfig_GetZipCode(char *buffer, size_t buffer_size);
esp_err_t DeviceConfig_SetDeviceName(const char *device_name);
esp_err_t DeviceConfig_SetZipCode(const char *zip);
esp_err_t DeviceConfig_GetWiFiCredentials(char *ssid, size_t ssid_size, char *password, size_t password_size);
esp_err_t DeviceConfig_SetWiFiCredentials(const char *ssid, const char *password);

#endif // DEVICE_CONFIG_H
