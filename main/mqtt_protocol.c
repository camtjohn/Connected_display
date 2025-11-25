#include "mqtt_protocol.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_PROTOCOL";

// Encode a message into wire format: [format][api_number][data_length][data...]
esp_err_t MQTT_Protocol_Encode(const mqtt_message_t *msg, uint8_t *buffer, size_t buffer_size) {
    // Validate data length
    if (msg->data_length > MQTT_MAX_PAYLOAD_SIZE) {
        ESP_LOGE(TAG, "Data length exceeds maximum: %d > %d", msg->data_length, MQTT_MAX_PAYLOAD_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    // Build message
    buffer[0] = (uint8_t)msg->format;
    buffer[1] = msg->api_number;
    buffer[2] = msg->data_length;
    
    if (msg->data_length > 0) {
        memcpy(&buffer[3], msg->data, msg->data_length);
    }

    ESP_LOGD(TAG, "Encoded message: format=%d, api=%d, length=%d", 
             msg->format, msg->api_number, msg->data_length);
    
    return ESP_OK;
}

// Decode a wire format message into structure
esp_err_t MQTT_Protocol_Decode(const uint8_t *buffer, size_t buffer_length, mqtt_message_t *msg) {
    // Parse header
    msg->format = (mqtt_format_t)buffer[0];
    msg->api_number = buffer[1];
    msg->data_length = buffer[2];

    // Copy data
    if (msg->data_length > 0) {
        memcpy(msg->data, &buffer[3], msg->data_length);
    }

    ESP_LOGD(TAG, "Decoded message: format=%d, api=%d, length=%d", 
             msg->format, msg->api_number, msg->data_length);

    return ESP_OK;
}

// Helper: Encode a single byte value
esp_err_t MQTT_Protocol_EncodeByte(uint8_t api_number, uint8_t value, uint8_t *buffer) {
    mqtt_message_t msg = {
        .format = MQTT_FORMAT_BYTE,
        .api_number = api_number,
        .data_length = 1,
    };
    msg.data[0] = value;
    
    return MQTT_Protocol_Encode(&msg, buffer, MQTT_MAX_PAYLOAD_SIZE + MQTT_HEADER_SIZE);
}

// Helper: Encode a string value
esp_err_t MQTT_Protocol_EncodeString(uint8_t api_number, const char *str, uint8_t *buffer) {
    size_t str_len = strlen(str);
    
    mqtt_message_t msg = {
        .format = MQTT_FORMAT_STRING,
        .api_number = api_number,
        .data_length = (uint8_t)str_len,
    };
    memcpy(msg.data, str, str_len);
    
    return MQTT_Protocol_Encode(&msg, buffer, MQTT_MAX_PAYLOAD_SIZE + MQTT_HEADER_SIZE);
}

// Helper: Encode a uint16 value (big-endian)
esp_err_t MQTT_Protocol_EncodeUint16(uint8_t api_number, uint16_t value, uint8_t *buffer) {
    mqtt_message_t msg = {
        .format = MQTT_FORMAT_UINT16,
        .api_number = api_number,
        .data_length = 2,
    };
    msg.data[0] = (value >> 8) & 0xFF;  // High byte
    msg.data[1] = value & 0xFF;         // Low byte
    
    return MQTT_Protocol_Encode(&msg, buffer, MQTT_MAX_PAYLOAD_SIZE + MQTT_HEADER_SIZE);
}

// Helper: Decode a byte value
esp_err_t MQTT_Protocol_DecodeByte(const mqtt_message_t *msg, uint8_t *value) {
    *value = msg->data[0];
    return ESP_OK;
}

// Helper: Decode a string value
esp_err_t MQTT_Protocol_DecodeString(const mqtt_message_t *msg, char *str, size_t str_size) {
    memcpy(str, msg->data, msg->data_length);
    str[msg->data_length] = '\0';  // Null terminate
    
    return ESP_OK;
}

// Helper: Decode a uint16 value (big-endian)
esp_err_t MQTT_Protocol_DecodeUint16(const mqtt_message_t *msg, uint16_t *value) {
    *value = ((uint16_t)msg->data[0] << 8) | msg->data[1];
    return ESP_OK;
}
