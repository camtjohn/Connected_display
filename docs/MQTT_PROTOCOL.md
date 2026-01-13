# MQTT Binary Protocol Implementation

## Overview
Successfully migrated MQTT messaging from string-based to binary protocol following the specification in `MQTT_PROTOCOL.md`. The protocol now also includes collaborative "Shared View" messages used by the Etchsketch canvas for multi-device drawing.

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

### Shared View Protocol

Collaborative drawing uses three message types on the `shared_view` topic:

#### 0x20 - Shared View Request (header only)
```
[0x20][0x00]
```
- No payload. Requests a full frame from the publisher.

#### 0x21 - Shared View Frame (payload: 2 + 16*2*3 bytes)
```
[0x21][len]
  [seq_hi][seq_lo]
  [red rows (16× uint16 row bitmasks)]
  [green rows (16× uint16 row bitmasks)]
  [blue rows (16× uint16 row bitmasks)]
```
- `seq`: 2-byte sequence number in network byte order (big-endian).
- Row bitmasks: 16 rows per color, 2 bytes per row; each bit represents one column (0–15). Bytes are transmitted as raw uint16 values; receivers memcpy into their internal `uint16_t[16]` buffers.

#### 0x22 - Shared View Updates (payload: 3 + N*3 bytes)
```
[0x22][len]
  [seq_hi][seq_lo]
  [count]
  repeat count times:
    [row][col][color]
```
- `seq`: 2-byte sequence number in network byte order (big-endian).
- `count`: number of pixel update triplets.
- Each update triplet contains 1-byte `row` (0–15), 1-byte `col` (0–15), and 1-byte `color` (0=red, 1=green, 2=blue).

### Sequencing & Sync
- Receivers track the last seen sequence. If an incoming `seq` is not exactly `last_seq + 1`, they log a gap and issue a `Shared View Request` to fetch a full frame.
- Full frame messages reset the local view and update the tracked sequence to the frame’s `seq`.

### Batching Behavior (Etchsketch)
- Local drawing uses a batching queue managed by `Etchsketch__Begin_batch()`, `Etchsketch__Queue_local_pixel()`, and `Etchsketch__End_batch()`.
- Pixels are queued as triplets; duplicate `(row,col)` entries update the color in-place.
- Flush triggers:
  - On `Etchsketch__End_batch()` (always flush any remaining queued pixels).
  - When the queue size reaches the threshold (`FLUSH_THRESHOLD = 8`), an updates message is published immediately.
- Each flush publishes a `Shared View Updates` message with the current `seq`, then increments the sender’s next sequence.

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

For shared view testing:
- **Topic**: `shared_view`
- **Payloads**: Use `0x21` for full frames and `0x22` for pixel updates, ensuring `seq` increments by 1 for each message.
- **Sync**: To simulate a gap, skip a `seq` value; the device will request a full frame with `0x20`.

## Notes

- Temperature offset (+50) only applies to current weather (MSG_TYPE_CURRENT_WEATHER)
- Forecast temperatures are stored directly without offset
- Moon phase uses 3 states: 0 (<93%), 1 (93-99%), 2 (100% full)
- Maximum payload size: 255 bytes
