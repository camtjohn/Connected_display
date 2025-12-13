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
#define MSG_TYPE_VERSION            0x10

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
 * @brief Version message payload
 * Total message size: 3 bytes (header + payload)
 */
typedef struct {
    uint8_t version;        // Firmware version number
} mqtt_version_t;

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

#endif // MQTT_PROTOCOL_H
