# Quick Start Guide - WiFi Provisioning Feature

## How to Trigger Provisioning

1. **Press and hold GPIO 0** (M5 Stamp S3 built-in button) for **5 seconds**
2. Device will enter provisioning mode automatically
3. LED matrix will show "P" indicator
4. WiFi access point "Connected_Display" will appear (password: "setup1234")

## What Happens During Provisioning

```
User Action         → Device Response
─────────────────────────────────────────
Hold GPIO 0 (5s)   → EVENT_UI_BUILTIN_BUTTON_LONGPRESS posted
                   → Provisioning view displayed
                   → SoftAP started
                   → HTTP server started on port 80
                   
Connect to AP      → User sees form at 192.168.4.1
Enter credentials  → Credentials saved to NVS
Submit form        → Device stops SoftAP/HTTP server
                   → Device reboots
                   → Device connects to configured WiFi
```

## File Structure

```
main/
├── provisioning.c              # Core provisioning module
├── Include/
│   └── provisioning.h          # Provisioning API header
├── Views/
│   ├── provisioning_view.c     # LED matrix display
│   └── Include/
│       └── provisioning_view.h # View API header
├── device_config.c             # WiFi credential storage (UPDATED)
├── Include/
│   ├── device_config.h         # Config API (UPDATED)
│   ├── event_system.h          # Event types (UPDATED)
│   ├── ui.h                    # Button timing (UPDATED)
├── ui.c                        # Button handler (UPDATED)
├── wifi.c                      # WiFi connection (UPDATED)
├── main.c                      # App flow (UPDATED)
└── CMakeLists.txt              # Build config (UPDATED)
```

## Key Configuration Constants

| Constant | Value | Location | Purpose |
|----------|-------|----------|---------|
| BTN_LONGPRESS_MS | 5000 | ui.h | Long press duration (5 seconds) |
| SOFTAP_SSID | "Connected_Display" | provisioning.c | WiFi AP name |
| SOFTAP_PASS | "setup1234" | provisioning.c | WiFi AP password |
| WIFI_SSID_MAX_LEN | 32 | provisioning.h | Max SSID length |
| WIFI_PASS_MAX_LEN | 64 | provisioning.h | Max password length |

## API Reference

### Provisioning Module

```c
// Start provisioning (blocks until credentials received)
int Provisioning__Start(void);

// Stop provisioning and cleanup
void Provisioning__Stop(void);

// Check if provisioning is active
int Provisioning__Is_Active(void);
```

### Device Config (WiFi)

```c
// Get WiFi credentials
int Device_Config__Get_WiFi_SSID(char *ssid, size_t max_len);
int Device_Config__Get_WiFi_Password(char *password, size_t max_len);

// Set WiFi credentials (saves to NVS)
int Device_Config__Set_WiFi_SSID(const char *ssid);
int Device_Config__Set_WiFi_Password(const char *password);
```

### Event Types

```c
typedef enum {
    // ... existing events ...
    EVENT_UI_BUILTIN_BUTTON_LONGPRESS,  // GPIO 0 held for 5 seconds
    // ... other events ...
} event_type_t;
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Provisioning not triggered | Button press < 5 seconds | Hold button for full 5 seconds |
| Can't connect to SoftAP | SoftAP not running | Check console for SoftAP startup messages |
| Can't access 192.168.4.1 | Not connected to AP | Verify connection to "Connected_Display" |
| Credentials not saved | NVS full or corrupted | Check device config initialization |
| Device won't reboot | Provisioning hung | Check HTTP server logs |

## Build & Deploy

```bash
# Build project
idf.py build

# Flash device
idf.py flash

# Monitor output
idf.py monitor
```

## Expected Log Output

```
...
I (xxx) PROVISIONING: Starting SoftAP: Connected_Display
I (xxx) PROVISIONING: SoftAP started. SSID: Connected_Display Password: setup1234
I (xxx) PROVISIONING: Connect to the network and open http://192.168.4.1 in your browser
I (xxx) PROVISIONING: Waiting for provisioning...
...
I (xxx) PROVISIONING: Provisioning completed successfully
I (xxx) MAIN: Rebooting to apply new WiFi credentials
```

## Notes

- Provisioning blocks the main application until credentials are received
- Once provisioning completes, device automatically reboots
- WiFi credentials are stored in NVS and survive power cycles
- SoftAP is disabled after provisioning completes
- Default SoftAP password is "setup1234" - change if deploying to end users
