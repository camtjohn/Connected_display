# MQTT Binary Protocol Implementation

## Overview
Successfully migrated MQTT messaging from string-based to binary protocol following the specification in `MQTT_PROTOCOL.md`.

## Files Created

### 1. `main/Include/mqtt_protocol.h`
Header file defining the binary protocol structure:
- **Message Types**: Constants for different message types (0x00, 0x01, 0x02, 0x10)
- **Data Structures**: 
  - `mqtt_msg_header_t` - Generic 2-byte header
  - `mqtt_current_weather_t` - Current weather payload
  - `mqtt_forecast_weather_t` - Multi-day forecast payload
  - `mqtt_version_t` - Version message payload
- **API Functions**: Parser functions for each message type

### 2. `main/mqtt_protocol.c`
Implementation of binary protocol parsing:
- `mqtt_protocol_parse_header()` - Parse 2-byte header
- `mqtt_protocol_parse_current_weather()` - Parse current temp with offset
- `mqtt_protocol_parse_forecast_weather()` - Parse multi-day forecast
- `mqtt_protocol_parse_version()` - Parse version message
- `mqtt_protocol_get_actual_temp()` - Convert offset temperature to actual value

## Files Modified

### 1. `main/mqtt.c`
Complete refactoring to handle binary messages:
- **Removed**: String parsing functions (`dbl_dig_str_to_int`, `add_null_to_str`, `update_weather_module`)
- **Added**: Binary message processing functions:
  - `process_current_weather()` - Process MSG_TYPE_CURRENT_WEATHER (0x01)
  - `process_forecast_weather()` - Process MSG_TYPE_FORECAST_WEATHER (0x02)
- **Updated**: 
  - `incoming_mqtt_handler()` - Now parses binary protocol with header validation
  - `compare_version()` - Takes uint8_t instead of string
  - Message deduplication using binary comparison instead of string comparison

### 2. `main/Include/mqtt.h`
- Updated `MQTT_TOPIC_WEATHER_UPDATE` from "weather49085" to "weather/49085" (matches protocol spec)
- Removed `MAX_NUM_PAYLOAD_BYTES` (now defined in mqtt_protocol.h as `MQTT_PROTOCOL_MAX_PAYLOAD`)

### 3. `main/CMakeLists.txt`
- Added `mqtt_protocol.c` to build sources

## Protocol Details

### Message Structure
```
[Type: 1 byte][Length: 1 byte][Payload: 0-255 bytes]
```

### Supported Messages

#### 0x01 - Current Weather (3 bytes total)
```
[0x01][0x01][temp]
```
- Temperature encoded with +50 offset to handle negative values
- Example: `0x01 0x01 0x46` = 20°F (70 - 50)

#### 0x02 - Forecast Weather (variable length)
```
[0x02][length][numDays][day1_high][day1_precip][day1_moon]...
```
- Each day: 3 bytes (high temp, precipitation %, moon phase)
- Supports up to 7 days
- Example: 3-day forecast = 2 + 1 + 9 = 12 bytes total

#### 0x10 - Version (3 bytes total)
```
[0x10][0x01][version]
```
- Version number for OTA update comparison

## Key Features

1. **Binary Protocol Compliance**: Fully implements spec from MQTT_PROTOCOL.md
2. **Robust Error Handling**: Validates message lengths and structure
3. **Logging**: Comprehensive ESP_LOG statements for debugging
4. **Backward Compatible Interface**: Weather module API unchanged
5. **Memory Efficient**: Stores raw binary for duplicate detection
6. **Type Safety**: Strong typing with dedicated structures for each message type

## Testing Considerations

When testing, the MQTT broker should send messages in this format:
- **Topic**: `weather/49085` for weather updates, `dev0` for version
- **Payload**: Binary data following protocol specification
- **Bootup**: Device publishes `"dev0,49085"` to `dev/bootup` on connection

## Notes

- Temperature offset (+50) only applies to current weather (MSG_TYPE_CURRENT_WEATHER)
- Forecast temperatures are stored directly without offset
- Moon phase uses 3 states: 0 (<93%), 1 (93-99%), 2 (100% full)
- Maximum payload size: 255 bytes
