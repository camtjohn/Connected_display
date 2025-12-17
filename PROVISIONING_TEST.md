# Testing Provisioning with New Parameters

## Quick Test

1. **Generate test NVS partition**:
```bash
python provision_certificates.py \
  --cert main/TLS_Keys/device002.crt \
  --key main/TLS_Keys/device002.key \
  --device-name "testdev" \
  --zipcode "12345" \
  --user-name "testuser" \
  --output test_nvs.bin
```

2. **Flash to device**:
```bash
esptool.py -p COM3 write_flash 0x9000 test_nvs.bin
```

3. **Verify in logs** (after reset):
```
I (xxx) DEVICE_CONFIG: Loaded device name from NVS: testdev
I (xxx) DEVICE_CONFIG: Loaded zipcode from NVS: 12345
I (xxx) DEVICE_CONFIG: Loaded user name from NVS: testuser
I (xxx) DEVICE_CONFIG: Device config initialized: testdev, 12345, testuser
I (xxx) DEVICE_CONFIG: Loaded client certificate from NVS (xxxx bytes)
I (xxx) DEVICE_CONFIG: Loaded client key from NVS (xxxx bytes)
```

## Example Usage in Code

```c
char device_name[16];
char zipcode[8];
char user_name[16];

Device_Config__Get_Name(device_name, sizeof(device_name));
Device_Config__Get_Zipcode(zipcode, sizeof(zipcode));
Device_Config__Get_UserName(user_name, sizeof(user_name));

ESP_LOGI(TAG, "Device: %s, Zip: %s, User: %s", 
         device_name, zipcode, user_name);
```

## Update at Runtime

```c
// Update any parameter
Device_Config__Set_Name("newdev");
Device_Config__Set_Zipcode("54321");
Device_Config__Set_UserName("newuser");

// Changes are immediately saved to NVS
```
