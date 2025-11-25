#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// Data format types
typedef enum {
    MQTT_FORMAT_BYTE = 0,
    MQTT_FORMAT_STRING = 1,
    MQTT_FORMAT_UINT16 = 2
} mqtt_format_t;

// API numbers
typedef enum {
    MQTT_API_HEARTBEAT = 0,           // Device alive ping
    MQTT_API_DEVICE_CHECKIN = 1,      // Device registration/config report
    MQTT_API_WEATHER_CURRENT = 2,     // Current weather update
    MQTT_API_WEATHER_FORECAST = 3,    // Forecast weather update
    MQTT_API_SHARE_ARTWORK = 4,       // Share display artwork
    MQTT_API_SEND_DEBUG_MSG= 5,       // Send debug msg from ap
    MQTT_API_SET_WIFI_CREDS = 6,      // Set WiFi credentials
    MQTT_API_SET_DEVICE_CONFIG = 7,   // Set device number/zip code
    MQTT_API_SET_VIEW = 8,            // Change active view
    MQTT_API_OTA_UPDATE = 9,          // Trigger OTA firmware update
    MQTT_API_REBOOT = 10,             // Reboot device
    MQTT_API_GET_STATUS = 11,         // Request device status report
} mqtt_api_number_t;

// Maximum sizes
#define MQTT_MAX_PAYLOAD_SIZE 128
#define MQTT_HEADER_SIZE 3  // format byte + api number + data length

// Message structure
typedef struct {
    mqtt_format_t format;
    uint8_t api_number;
    uint8_t data_length;
    uint8_t data[MQTT_MAX_PAYLOAD_SIZE];
} mqtt_message_t;

// Public API
esp_err_t MQTT_Protocol_Encode(const mqtt_message_t *msg, uint8_t *buffer, size_t buffer_size);
esp_err_t MQTT_Protocol_Decode(const uint8_t *buffer, size_t buffer_length, mqtt_message_t *msg);

// Helper functions for common data types
esp_err_t MQTT_Protocol_EncodeByte(uint8_t api_number, uint8_t value, uint8_t *buffer);
esp_err_t MQTT_Protocol_EncodeString(uint8_t api_number, const char *str, uint8_t *buffer);
esp_err_t MQTT_Protocol_EncodeUint16(uint8_t api_number, uint16_t value, uint8_t *buffer);

esp_err_t MQTT_Protocol_DecodeByte(const mqtt_message_t *msg, uint8_t *value);
esp_err_t MQTT_Protocol_DecodeString(const mqtt_message_t *msg, char *str, size_t str_size);
esp_err_t MQTT_Protocol_DecodeUint16(const mqtt_message_t *msg, uint16_t *value);

#endif // MQTT_PROTOCOL_H
