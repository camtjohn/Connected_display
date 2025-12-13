#include "mqtt_protocol.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "MQTT_PROTOCOL";

int mqtt_protocol_parse_header(const uint8_t *data, uint8_t data_len, mqtt_msg_header_t *header) {
    if (data == NULL || header == NULL) {
        ESP_LOGE(TAG, "NULL pointer passed to parse_header");
        return -1;
    }
    
    header->type = data[0];
    header->length = data[1];
    
    // Validate that we have enough data for the payload
    if (data_len < MQTT_PROTOCOL_HEADER_SIZE + header->length) {
        ESP_LOGE(TAG, "Data length %d insufficient for payload length %d", 
                 data_len, header->length);
        return -1;
    }
    
    ESP_LOGI(TAG, "Parsed header: type=0x%02X, length=%d", header->type, header->length);
    return 0;
}

int mqtt_protocol_parse_current_weather(const uint8_t *payload, uint8_t payload_len, 
                                        mqtt_current_weather_t *weather) {
    if (payload == NULL || weather == NULL) {
        ESP_LOGE(TAG, "NULL pointer passed to parse_current_weather");
        return -1;
    }
    
    if (payload_len < 1) {
        ESP_LOGE(TAG, "Payload too short for current weather");
        return -1;
    }
    
    weather->temperature = payload[0];
    
    int8_t actual_temp = mqtt_protocol_get_actual_temp(weather->temperature);
    ESP_LOGI(TAG, "Current weather: temp=%d°F (raw=0x%02X)", actual_temp, weather->temperature);
    
    return 0;
}

int mqtt_protocol_parse_forecast_weather(const uint8_t *payload, uint8_t payload_len,
                                         mqtt_forecast_weather_t *forecast) {
    if (payload == NULL || forecast == NULL) {
        ESP_LOGE(TAG, "NULL pointer passed to parse_forecast_weather");
        return -1;
    }
    
    if (payload_len < 1) {
        ESP_LOGE(TAG, "Payload too short for forecast weather");
        return -1;
    }
    
    forecast->num_days = payload[0];
    
    // Validate payload length
    uint8_t expected_len = 1 + (forecast->num_days * 3);
    if (payload_len < expected_len) {
        ESP_LOGE(TAG, "Payload length %d insufficient for %d days (expected %d)", 
                 payload_len, forecast->num_days, expected_len);
        return -1;
    }
    
    if (forecast->num_days > 7) {
        ESP_LOGW(TAG, "Too many forecast days (%d), limiting to 7", forecast->num_days);
        forecast->num_days = 7;
    }
    
    ESP_LOGI(TAG, "Parsing %d-day forecast", forecast->num_days);
    
    // Parse each day's data
    for (uint8_t i = 0; i < forecast->num_days; i++) {
        uint8_t offset = 1 + (i * 3);
        forecast->days[i].high_temp = payload[offset];
        forecast->days[i].precip = payload[offset + 1];
        forecast->days[i].moon = payload[offset + 2];
        
        ESP_LOGI(TAG, "  Day %d: %d°F, %d%% precip, moon=%d", 
                 i + 1, 
                 forecast->days[i].high_temp,
                 forecast->days[i].precip,
                 forecast->days[i].moon);
    }
    
    return 0;
}

int mqtt_protocol_parse_version(const uint8_t *payload, uint8_t payload_len,
                                mqtt_version_t *version) {
    if (payload == NULL || version == NULL) {
        ESP_LOGE(TAG, "NULL pointer passed to parse_version");
        return -1;
    }
    
    if (payload_len < 1) {
        ESP_LOGE(TAG, "Payload too short for version");
        return -1;
    }
    
    version->version = payload[0];
    
    ESP_LOGI(TAG, "Version: %d", version->version);
    
    return 0;
}

int8_t mqtt_protocol_get_actual_temp(uint8_t temp_with_offset) {
    return (int8_t)(temp_with_offset - TEMP_OFFSET);
}
