#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H

#include <stdint.h>

/**
 * MQTT Binary Message Protocol
 * 
 * All MQTT messages use a binary protocol with a 2-byte header followed by
 * a variable-length payload (0-255 bytes).
 * 
 * Message Structure:
 * [Type: 1 byte][Length: 1 byte][Payload: 0-255 bytes]
 */

// Message Type Definitions
#define MSG_TYPE_GENERIC            0x00
#define MSG_TYPE_CURRENT_WEATHER    0x01
#define MSG_TYPE_FORECAST_WEATHER   0x02
#define MSG_TYPE_DEVICE_CONFIG      0x03
#define MSG_TYPE_VERSION            0x10
#define MSG_TYPE_HEARTBEAT          0x11
#define MSG_TYPE_SHARED_VIEW_REQ    0x20
#define MSG_TYPE_SHARED_VIEW_FRAME  0x21


// Protocol Constants
#define MQTT_PROTOCOL_HEADER_SIZE   2
#define MQTT_PROTOCOL_MAX_PAYLOAD   255

// Temperature offset for current weather (to handle negative temps as uint8)
#define TEMP_OFFSET                 50

// Moon phase constants
#define MOON_PHASE_LESS_THAN_93     0
#define MOON_PHASE_93_TO_99         1
#define MOON_PHASE_FULL             2

/**
 * @brief Generic message header structure
 */
typedef struct {
    uint8_t type;       // Message type
    uint8_t length;     // Payload length (0-255 bytes)
} mqtt_msg_header_t;

/**
 * @brief Current weather message payload
 * Total message size: 3 bytes (header + payload)
 */
typedef struct {
    uint8_t temperature;    // Temperature in °F with +50 offset
} mqtt_current_weather_t;

/**
 * @brief Forecast day data (3 bytes per day)
 */
typedef struct {
    uint8_t high_temp;      // High temperature in °F (no offset)
    uint8_t precip;         // Precipitation probability (0-100%)
    uint8_t moon;           // Moon phase indicator (0, 1, or 2)
} mqtt_forecast_day_t;

/**
 * @brief Forecast weather message payload
 * Variable size: 1 + (num_days * 3) bytes
 */
typedef struct {
    uint8_t num_days;                   // Number of forecast days
    mqtt_forecast_day_t days[7];        // Array of forecast days (max 7)
} mqtt_forecast_weather_t;

/**
 * @brief Device config message payload
 * Format: [num_strings][str1_len][str1_data][str2_len][str2_data]...\n * Supports variable number of variable-length strings
 */
typedef struct {
    uint8_t num_strings;
    char strings[10][16];       // Up to 10 strings, max 16 chars each
    uint8_t string_lens[10];
} mqtt_device_config_t;

/**
 * @brief Version message payload
 * Total message size: 3 bytes (header + payload)
 */
typedef struct {
    uint16_t version;        // Firmware version number
} mqtt_version_t;

typedef struct {
    uint16_t seq;         // Sequence number for gap detection
    uint16_t red[16];
    uint16_t green[16];
    uint16_t blue[16];
} mqtt_shared_view_frame_t;

typedef struct {
    uint8_t row : 4;      // 4-bit row (0-15)
    uint8_t col : 4;      // 4-bit col (0-15)
    uint8_t color;        // 0=red, 1=green, 2=blue
} mqtt_shared_pixel_update_t;

/**
 * @brief Parse MQTT binary message header
 * 
 * @param data Pointer to raw MQTT message data
 * @param data_len Length of data buffer
 * @param header Pointer to header structure to populate
 * @return 0 on success, -1 on error
 */
int mqtt_protocol_parse_header(const uint8_t *data, uint8_t data_len, mqtt_msg_header_t *header);

/**
 * @brief Parse current weather message
 * 
 * @param payload Pointer to payload data (after header)
 * @param payload_len Length of payload
 * @param weather Pointer to current weather structure to populate
 * @return 0 on success, -1 on error
 */
int mqtt_protocol_parse_current_weather(const uint8_t *payload, uint8_t payload_len, 
                                        mqtt_current_weather_t *weather);

/**
 * @brief Parse forecast weather message
 * 
 * @param payload Pointer to payload data (after header)
 * @param payload_len Length of payload
 * @param forecast Pointer to forecast structure to populate
 * @return 0 on success, -1 on error
 */
int mqtt_protocol_parse_forecast_weather(const uint8_t *payload, uint8_t payload_len,
                                         mqtt_forecast_weather_t *forecast);

/**
 * @brief Parse version message
 * 
 * @param payload Pointer to payload data (after header)
 * @param payload_len Length of payload
 * @param version Pointer to version structure to populate
 * @return 0 on success, -1 on error
 */
int mqtt_protocol_parse_version(const uint8_t *payload, uint8_t payload_len,
                                mqtt_version_t *version);

/**
 * @brief Get actual temperature from current weather with offset
 * 
 * @param temp_with_offset Temperature value from message (uint8)
 * @return Actual temperature in °F (can be negative)
 */
int8_t mqtt_protocol_get_actual_temp(uint8_t temp_with_offset);

/**
 * @brief Build device config message with multiple variable-length strings
 * Encodes strings as: [num_strings][str1_len][str1][str2_len][str2]...
 * 
 * @param strings Array of string pointers
 * @param num_strings Number of strings in array
 * @param buffer Output buffer for complete message (header + payload)
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int mqtt_protocol_build_device_config(const char **strings, uint8_t num_strings,
                                      uint8_t *buffer, uint8_t buffer_size);

/**
 * @brief Build heartbeat message with device name
 * 
 * @param device_name Device name string
 * @param buffer Output buffer for complete message (header + payload)
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int mqtt_protocol_build_heartbeat(const char *device_name, uint8_t *buffer, uint8_t buffer_size);

int mqtt_protocol_build_shared_view_request(uint8_t *buffer, uint8_t buffer_size);
int mqtt_protocol_parse_shared_view_frame(const uint8_t *payload, uint8_t payload_len,
                                          mqtt_shared_view_frame_t *frame);
int mqtt_protocol_build_shared_view_updates(const mqtt_shared_pixel_update_t *updates, uint8_t num_updates,
                                            uint16_t seq,
                                            uint8_t *buffer, uint8_t buffer_size);
int mqtt_protocol_parse_shared_view_updates(const uint8_t *payload, uint8_t payload_len,
                                            mqtt_shared_pixel_update_t *updates, uint8_t max_updates, 
                                            uint8_t *out_count, uint16_t *out_seq);

#endif // MQTT_PROTOCOL_H
