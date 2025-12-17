# TLS Certificate Provisioning Guide

## Overview

Starting with this version, the device firmware only embeds the **CA certificate** in the binary. The **client certificate** and **client key** are stored in NVS (Non-Volatile Storage) and must be provisioned separately for each device.

## Why This Change?

- **Device-specific credentials**: Each device can have its own unique certificate and key
- **Security**: Certificates are not stored in the firmware binary
- **Flexibility**: Update certificates without rebuilding firmware
- **Smaller firmware**: Reduced binary size

## Architecture

### Embedded in Firmware (Binary)
- CA certificate (`ca.crt`) - Used to verify the MQTT broker

### Stored in NVS (Flash)
- Client certificate (`device002.crt`) - Device identity certificate
- Client key (`device002.key`) - Private key for device authentication

## Provisioning Methods

### Method 1: Automated Script (Recommended)

#### Linux/Mac:
```bash
# Make script executable
chmod +x provision_certificates.sh

# Basic usage with default files
./provision_certificates.sh --port /dev/ttyUSB0

# With custom files
./provision_certificates.sh \
  --cert path/to/client.crt \
  --key path/to/client.key \
  --port /dev/ttyUSB0

# With device configuration
./provision_certificates.sh \
  --cert path/to/client.crt \
  --key path/to/client.key \
  --device-name "device001" \
  --zipcode "60607" \
  --user-name "john_doe" \
  --port /dev/ttyUSB0
```

#### Windows (PowerShell):
```powershell
# Basic usage
.\provision_certificates.ps1 -Port COM3

# With custom files
.\provision_certificates.ps1 `
  -CertFile "path\to\client.crt" `
  -KeyFile "path\to\client.key" `
  -Port COM3
```

The PowerShell script will generate the NVS CSV file and provide instructions for flashing.

### Method 2: Python Script

```bash
# Generate NVS partition binary
python provision_certificates.py \
  --cert main/TLS_Keys/device002.crt \
  --key main/TLS_Keys/device002.key \
  --device-name "device001" \
  --zipcode "60607" \
  --user-name "john_doe" \
  --output nvs_certs.bin

# Flash to device
esptool.py -p COM3 write_flash 0x9000 nvs_certs.bin
```

### Method 3: Manual Steps

1. **Create NVS CSV file** (`nvs_certs.csv`):
```csv
key,type,encoding,value
device_cfg,namespace,,
client_cert,file,binary,/absolute/path/to/client.crt
client_key,file,binary,/absolute/path/to/client.key
name,data,string,device001
zipcode,data,string,60607
user_name,data,string,john_doe
```

2. **Generate NVS partition binary**:
```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
  generate nvs_certs.csv nvs_certs.bin 0x6000
```

3. **Flash to device**:
```bash
esptool.py -p COM3 write_flash 0x9000 nvs_certs.bin
```

## Partition Information

From `partitions.csv`:
- **NVS Offset**: 0x9000
- **NVS Size**: 0x6000 (24 KB)

## NVS Keys

The following keys are used in the `device_cfg` namespace:

| Key | Type | Description |
|-----|------|-------------|
| `client_cert` | blob | Client TLS certificate (PEM format) |
| `client_key` | blob | Client TLS private key (PEM format) |
| `name` | string | Device name (8 chars max) |
| `zipcode` | string | Device zipcode (8 chars max) |
| `user_name` | string | User name (16 chars max) |

## Verification

After provisioning, monitor the serial output during boot:

```
I (1234) DEVICE_CONFIG: Loaded client certificate from NVS (1234 bytes)
I (1240) DEVICE_CONFIG: Loaded client key from NVS (1675 bytes)
I (1250) MQTT: Loaded client certificate (1234 bytes) and key (1675 bytes) from NVS
```

## Troubleshooting

### "Client certificate not found in NVS"

This error means the NVS partition doesn't contain the certificate. Solutions:

1. Verify you flashed the NVS partition to the correct offset (0x9000)
2. Check that the CSV file was generated correctly
3. Try erasing the NVS partition and reflashing:
   ```bash
   esptool.py -p COM3 erase_region 0x9000 0x6000
   esptool.py -p COM3 write_flash 0x9000 nvs_certs.bin
   ```

### "Failed to allocate memory for client certificate"

The certificate files may be too large. Check:
- Certificate file size (should be < 2KB typically)
- Key file size (should be < 2KB typically)
- Available heap memory

### Permission Issues (Linux)

If you get permission errors accessing the serial port:
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in for changes to take effect
```

## Development Workflow

### For Each New Device:

1. Generate device-specific certificate and key (or use existing)
2. Run provisioning script:
   ```bash
   ./provision_certificates.sh \
     --cert path/to/deviceXXX.crt \
     --key path/to/deviceXXX.key \
     --device-name "deviceXXX" \
     --port /dev/ttyUSB0
   ```
3. Reset the device
4. Verify MQTT connection in logs

### For Firmware Updates:

The certificates in NVS are preserved across firmware updates. You only need to reprovision if:
- You want to update the certificates
- You erase the entire flash
- You change the NVS partition layout

## Security Considerations

1. **Private Key Protection**: Never commit private keys to version control
2. **Certificate Rotation**: Plan for certificate expiration and rotation
3. **Backup**: Keep backup copies of device certificates in a secure location
4. **Access Control**: Restrict access to the provisioning tools and certificate files

## API Reference

The following functions are available in `device_config.h`:

```c
// Get device configuration
int Device_Config__Get_Name(char *name, size_t max_len);
int Device_Config__Get_Zipcode(char *zipcode, size_t max_len);
int Device_Config__Get_UserName(char *user_name, size_t max_len);

// Get client certificate from NVS
int Device_Config__Get_Client_Cert(char *cert_buffer, size_t buffer_size, size_t *actual_size);

// Get client key from NVS
int Device_Config__Get_Client_Key(char *key_buffer, size_t buffer_size, size_t *actual_size);

// Set device configuration
int Device_Config__Set_Name(const char *name);
int Device_Config__Set_Zipcode(const char *zipcode);
int Device_Config__Set_UserName(const char *user_name);

// Set client certificate in NVS
int Device_Config__Set_Client_Cert(const char *cert, size_t cert_len);

// Set client key in NVS
int Device_Config__Set_Client_Key(const char *key, size_t key_len);
```

## File Structure

```
Connected_display/
├── main/
│   ├── TLS_Keys/
│   │   ├── ca.crt              # CA cert (embedded in firmware)
│   │   ├── device002.crt       # Example client cert (for provisioning)
│   │   └── device002.key       # Example client key (for provisioning)
│   ├── device_config.c         # NVS certificate management
│   └── mqtt.c                  # Loads certs from NVS
├── provision_certificates.sh   # Linux/Mac provisioning script
├── provision_certificates.ps1  # Windows provisioning script
├── provision_certificates.py   # Python provisioning tool
└── CERTIFICATE_PROVISIONING.md # This file
```

## Next Steps

After setting up certificate provisioning, you may want to:

1. Create a certificate generation script for your CA
2. Set up a certificate management database
3. Implement certificate rotation
4. Add HTTP endpoint for over-the-air certificate updates
