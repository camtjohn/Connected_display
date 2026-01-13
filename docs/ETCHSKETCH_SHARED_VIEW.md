# Etchsketch Shared View Protocol

## Overview

The Etchsketch Shared View is a collaborative drawing canvas (16×16 pixels, 3 color channels: red, green, blue) synchronized across multiple ESP32 devices via MQTT. The server maintains the canonical state and broadcasts updates and full frames to all connected devices.

## MQTT Topics

### Debug Build
- `debug_shared_view` — shared drawing canvas topic

### Production Build
- `shared_view` — shared drawing canvas topic

All devices publish and subscribe to the same topic.

## Message Types

All messages use a 2-byte binary header:
- **Byte 0**: Message type
- **Byte 1**: Payload length (0–255 bytes)
- **Bytes 2+**: Payload (variable)

### MSG_TYPE_SHARED_VIEW_REQ (0x20)
**Device → Server**: Request for full canvas state.

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| 0 | type | u8 | 0x20 |
| 1 | length | u8 | 0 (no payload) |

**Header only; no payload.**

---

### MSG_TYPE_SHARED_VIEW_FRAME (0x21)
**Server → Devices**: Full canvas state (16×16 for each color channel).

**Publish with `retain=1` on `shared_view` topic.**

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| 0 | type | u8 | 0x21 |
| 1 | length | u8 | 98 (fixed) |
| 2–3 | seq | u16 | Sequence number (network byte order, big-endian) |
| 4–35 | red[16] | u16[16] | Red channel bitmask for each of 16 rows (network byte order) |
| 36–67 | green[16] | u16[16] | Green channel bitmask for each of 16 rows (network byte order) |
| 68–99 | blue[16] | u16[16] | Blue channel bitmask for each of 16 rows (network byte order) |

**Total message size**: 2 (header) + 98 (payload) = 100 bytes

**Bit encoding**: Each row is a 16-bit mask where bit *i* represents column *i*. Bit value 1 = pixel on, 0 = pixel off.

**Network byte order**: All multi-byte integers (seq, u16[16] arrays) use big-endian (network byte order) for cross-platform compatibility.

---

### MSG_TYPE_SHARED_VIEW_UPDATES (0x22)
**Device → Server / Server → Devices**: Batched pixel updates.

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| 0 | type | u8 | 0x22 |
| 1 | length | u8 | 3 + (count × 3) |
| 2–3 | seq | u16 | Sequence number (network byte order, big-endian) |
| 4 | count | u8 | Number of pixel updates (1–32) |
| 5+ | pixels | struct[count] | Array of pixel updates |

**Pixel Update Structure** (3 bytes each):
| Offset | Field | Type | Range |
|--------|-------|------|-------|
| 0 | row | u8 | 0–15 |
| 1 | col | u8 | 0–15 |
| 2 | color | u8 | 0 (red), 1 (green), 2 (blue) |

**Server behavior**: On receiving device updates:
1. Apply pixel updates to canonical state.
2. Republish full frame as retained message with incremented `seq`.
3. Broadcast pixel updates to all other devices with the new `seq`.

**Device behavior**: On receiving updates:
1. Check `seq` against `last_seq_seen`.
2. If `seq != last_seq_seen + 1`, log warning and request full sync (gap detected).
3. Apply pixel updates locally.
4. Update `last_seq_seen = seq`.

---

## Sequence Numbers

**Purpose**: Detect dropped or out-of-order messages.

- **Scope**: `seq` is a monotonically increasing 16-bit counter maintained by the server.
- **Increment**: Server increments `seq` for every broadcast (both full frames and update batches).
- **Device tracking**: Each device maintains `last_seq_seen` and `next_seq_to_send`:
  - `last_seq_seen`: Last sequence number received in a frame or update batch from the server.
  - `next_seq_to_send`: Sequence number to include in the device's next outgoing update batch.

**Gap detection**:
- On receiving an update batch with `seq != last_seq_seen + 1`, the device:
  1. Logs a warning with expected and received seq values.
  2. Calls `Etchsketch__Shared_Request_full_sync()` to request a full frame.
  3. Continues processing (does not discard the current batch).

**On full frame sync**:
- Device resets `last_seq_seen = frame.seq`.
- Device may optionally skip intermediate updates and use the full frame as ground truth.

---

## Batching & Flushing

**Device batching**:
- Devices collect pixel updates in a local buffer while in "paint mode" (button held).
- **Flush triggers**:
  1. Button release (end of paint stroke).
  2. Buffer reaches 8 pending updates (`FLUSH_THRESHOLD`).
  3. Explicit `End_batch()` call.

**Burst protection**:
- Maximum 32 pending updates per batch (`MAX_PENDING_UPDATES`).
- If buffer fills beyond threshold, flush immediately to prevent message loss.

**Deduplication**:
- If the same pixel is painted multiple times in one batch, only the last color is sent.
- Updates travel in the order they were applied.

---

## Retained Message Strategy

**Why retained**: New devices (or devices that reconnect) receive the latest canvas state immediately from the broker without a synchronous request-response.

**Implementation**:
- Server publishes the full frame with `retain=1` on `shared_view` topic after each update.
- Broker stores this single retained message.
- New subscribers receive it automatically.
- To force a refresh, a device can publish a `MSG_TYPE_SHARED_VIEW_REQ` and the server republishes the current retained frame.

**Multi-device sync**:
- All devices share the same `shared_view` topic.
- Retained frame is overwritten on each server update, so it always reflects the latest state.
- Devices entering the view call `Etchsketch__On_Enter()` → `Etchsketch__Shared_Request_full_sync()` to ensure they have the current state.

---

## QoS & Delivery Guarantees

- **Update batches**: QoS 1 (at-least-once) to minimize loss during device paints.
- **Full frames**: QoS 1 (at-least-once); also retained by broker.
- **Sync requests**: QoS 0 (best effort); non-critical, loss is benign.

---

## Example Flows

### Device A Paints a Stroke

1. User holds button; device enters paint mode.
   - `Etchsketch__Shared_Begin_batch()` sets `Batch_active = 1`.
2. User moves cursor + paints pixels (8 pixels).
   - Each pixel call `Etchsketch__Shared_Apply_local_pixel()`.
   - Local canvas updates immediately.
   - Pixels added to `Pending_updates` buffer.
3. User releases button.
   - `Etchsketch__Shared_End_batch()` triggers flush.
4. Device publishes update batch with:
   - `MSG_TYPE_SHARED_VIEW_UPDATES`
   - `seq = next_seq_to_send` (e.g., 42)
   - 8 pixel triplets
   - Device increments `next_seq_to_send` to 43.
5. Server receives batch:
   - Applies 8 pixels to canonical state.
   - Republishes full frame with `seq = 42` (server-side seq, incremented by server).
   - Broadcasts updated batch to other devices.
6. Device B receives:
   - Full frame: checks `seq = 42` against `last_seq_seen`; updates.
   - Pixel updates: checks `seq = 42` against `last_seq_seen + 1`; matches, applies.

### Device C Joins / Reconnects

1. Device C subscribes to `shared_view`.
2. Broker delivers retained full frame (latest from server, e.g., `seq = 42`).
3. Device C applies full frame locally; sets `last_seq_seen = 42`.
4. Device C calls `Etchsketch__On_Enter()` (on view switch), which calls `Etchsketch__Shared_Request_full_sync()`.
   - This publishes a sync request.
   - Server responds by republishing the current retained frame (ensuring C has the absolute latest).

### Seq Gap Detection

1. Device A is offline during server updates (packets 100–102 lost).
2. Device A comes back online; broker delivers retained frame with `seq = 103`.
3. Device A sets `last_seq_seen = 103`.
4. Server sends next update with `seq = 104`.
5. Device A receives `seq = 104`, checks: `104 != 103 + 1`? No, it matches.
   - Device A applies the update.
6. However, if Device A *had* missed the retained frame and only saw packets 100, 101, then 104:
   - Receives `seq = 100`, sets `last_seq_seen = 100`.
   - Receives `seq = 101`, checks: `101 == 100 + 1`? Yes, applies.
   - Receives `seq = 104`, checks: `104 != 101 + 1`? Yes, gap detected!
   - Logs warning, calls `Etchsketch__Shared_Request_full_sync()`.
   - Server republishes retained frame with latest `seq`.

---

## Protocol Summary Table

| Message | Type | Dir | Size | QoS | Retained | Purpose |
|---------|------|-----|------|-----|----------|---------|
| Sync Request | 0x20 | D→S | 2 B | 0 | No | Device requests full canvas |
| Full Frame | 0x21 | S→D | 100 B | 1 | **Yes** | Server broadcasts canonical state |
| Updates | 0x22 | D↔S | 5–101 B | 1 | No | Pixel update batches |

**D = Device, S = Server, D↔S = Both directions**

---

## Implementation Checklist for Server

- [ ] Maintain canonical 16×16 canvas state (3 color channels as u16[16] bitmasks).
- [ ] On receiving device sync request (0x20), republish retained full frame.
- [ ] On receiving device updates (0x22):
  - [ ] Parse seq and pixel triplets.
  - [ ] Apply pixels to canonical state.
  - [ ] Increment server's seq counter.
  - [ ] Republish full frame with new seq (retain=1).
  - [ ] Broadcast pixel updates to all other devices with new seq.
- [ ] Track seq atomically to avoid race conditions in multi-threaded server.
- [ ] Use network byte order (big-endian) for all u16 and u16[16] fields.
- [ ] Publish with QoS 1 for reliability.

---

## Device-Side Implementation (ESP32)

**Included in this repo**: `main/Views/etchsketch.c`, `main/mqtt_protocol.c`, `main/mqtt.c`

**Key functions**:
- `Etchsketch__Shared_Request_full_sync()` — Device publishes sync request.
- `Etchsketch__Shared_Apply_remote_frame()` — Device applies full frame, resets seq.
- `Etchsketch__Shared_Apply_remote_updates()` — Device applies pixel batch, checks for gaps.
- `Etchsketch__Shared_Apply_local_pixel()` — Device paints locally, adds to batch.
- `Etchsketch__Shared_Begin_batch()` / `Etchsketch__Shared_End_batch()` — Batch paint strokes.

All public APIs are exposed in `main/Views/Include/etchsketch.h`.
