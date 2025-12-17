# Quick Reference: Device Configuration Storage

## What Was Implemented
✅ Persistent storage for device name, zipcode, and firmware version using ESP32 NVS
✅ Automatic version tracking on OTA updates  
✅ Write optimization to minimize flash wear
✅ In-memory caching for fast reads
✅ Integration with MQTT bootup messages

## Usage

### Initialize (done automatically in main.c)
```c
if (Device_Config__Init() != 0) {
    ESP_LOGE(TAG, "Failed to initialize device config!");
}
```

### Read Configuration
```c
char device_name[16];
char zipcode[8];
uint8_t version;

Device_Config__Get_Name(device_name, sizeof(device_name));
Device_Config__Get_Zipcode(zipcode, sizeof(zipcode));
version = Device_Config__Get_Version();

ESP_LOGI(TAG, "Device: %s, Zip: %s, Version: %d", device_name, zipcode, version);
```

### Update Configuration
```c
// Update device name
Device_Config__Set_Name("newname");

// Update zipcode
Device_Config__Set_Zipcode("12345");

// Update version (typically done automatically by OTA)
Device_Config__Set_Version(2);
```

## How It Works

### Boot Sequence
1. `main.c` calls `Device_Config__Init()`
2. NVS is initialized (or formatted if first boot)
3. Configuration values are loaded from flash
4. If not found, defaults are used and saved
5. Values are cached in RAM

### MQTT Bootup Message
1. When MQTT connects, it reads device name and zipcode from NVS
2. Builds binary device config message
3. Publishes to bootup topic with current configuration

### OTA Version Tracking
1. After successful OTA, device restarts
2. `OTA__Init()` detects first boot after update
3. Automatically increments version number
4. Saves new version to NVS
5. Marks new firmware as valid (prevents rollback)

## Default Values
- **Device Name**: "dev0"
- **Zipcode**: "60607"
- **Version**: 1

## Flash Wear
ESP32 NVS has built-in wear leveling:
- Supports 100,000+ erase cycles per sector
- Automatically distributes writes across sectors
- With typical usage (1 OTA/month), lifetime is effectively unlimited

## Testing

### Test Persistence Across Reboots
```bash
# Monitor initial boot
idf.py monitor

# Should see: "Device config initialized: dev0, 60607, v1"

# Reset device (Ctrl+T, Ctrl+R in monitor)
# Should see same values on reboot
```

### Test OTA Version Update
```bash
# Trigger OTA update (via button or MQTT)
# After restart, monitor should show:
# "First boot after OTA update detected"
# "Version updated from 1 to 2 after OTA"
```

### Test Config Changes
```c
// Add to your code temporarily
Device_Config__Set_Name("testdev");
Device_Config__Set_Zipcode("54321");

// Reboot device
// MQTT bootup message should show new values
```

## API Reference

### Initialization
```c
int Device_Config__Init(void);
```
Returns: 0 on success, -1 on failure

### Getters
```c
int Device_Config__Get_Name(char *name, size_t max_len);
int Device_Config__Get_Zipcode(char *zipcode, size_t max_len);
uint8_t Device_Config__Get_Version(void);
```
Returns: 0 on success (or version number), -1 on failure

### Setters
```c
int Device_Config__Set_Name(const char *name);
int Device_Config__Set_Zipcode(const char *zipcode);
int Device_Config__Set_Version(uint8_t version);
```
Returns: 0 on success, -1 on failure

Note: Setters only write to NVS if the value has changed (wear optimization)

## Troubleshooting

### "Config not initialized" Warning
- Ensure `Device_Config__Init()` is called before using getters/setters
- Check that NVS flash initialization succeeded

### Values Not Persisting
- Check NVS partition in partition table
- Verify flash is not write-protected
- Check for NVS errors in logs

### OTA Version Not Updating
- Ensure OTA completed successfully
- Check for "First boot after OTA" log message
- Verify partition state is `ESP_OTA_IMG_PENDING_VERIFY`

## Future Enhancements
- Add MQTT handler to receive config updates from server
- Add serial console commands for config management
- Add web interface for config editing
- Implement config validation (e.g., zipcode format)
- Support multiple weather locations
