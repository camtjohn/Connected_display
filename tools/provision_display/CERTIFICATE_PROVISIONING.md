# TLS Certificate Provisioning Guide

## Overview

All TLS certificates are now stored in NVS (Non-Volatile Storage) and must be provisioned separately for each device. Nothing is embedded in the firmware binary.

## Why This Change?

- **Device-specific credentials**: Each device can have its own unique certificate, key, and CA
- **Security**: Certificates are not stored in the firmware binary
- **Flexibility**: Update certificates without rebuilding firmware
- **Smaller firmware**: Reduced binary size
- **CA rotation**: Update CA certificate without reflashing firmware

## Architecture

### Stored in NVS (Flash)
- CA certificate (`ca.crt`) - Used to verify the MQTT broker
- Client certificate (`device002.crt`) - Device identity certificate  
- Client key (`device002.key`) - Private key for device authentication

### Certificate Location
All certificates to be provisioned are stored in the `certs_to_provision/` directory:
- `certs_to_provision/ca.crt`
- `certs_to_provision/device002.crt`
- `certs_to_provision/device002.key`

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
  --ca path/to/ca.crt \
  --cert path/to/client.crt \
  --key path/to/client.key \
  --port /dev/ttyUSB0

# With device configuration
./provision_certificates.sh \
  --ca path/to/ca.crt \
  --cert path/to/client.crt \
  --key path/to/client.key \
  --device-name "device001" \
  --zipcode "60607" \
  --user-name "john_doe" \
  --port /dev/ttyUSB0
```

#### Windows (PowerShell):
```powershell
# Basic usage (auto-detects certs from certs_to_provision/)
.\provision_certificates.ps1 -Port COM3

# With custom certificate path
.\provision_certificates.ps1 `
  -CaFile "path\to\ca.crt" `
  -CertFile "path\to\device.crt" `
  -KeyFile "path\to\device.key" `
  -Port COM3

# With device configuration
.\provision_certificates.ps1 `
  -DeviceName "device001" `
  -Zipcode "60607" `
  -UserName "john_doe" `
  -Port COM3
```

The PowerShell script will:
- Auto-detect `ca.crt`, `device*.crt`, and `device*.key` from the `certs_to_provision/` directory
- Generate the NVS CSV file
- Call the Python script to create the NVS binary
- Provide instructions for flashing

### Method 2: Python Script

```bash
# Basic usage (auto-detects certs from certs_to_provision/)
python provision_certificates.py -p COM3

# With custom certificate paths
python provision_certificates.py \
  --ca path/to/ca.crt \
  --cert path/to/device.crt \
  --key path/to/device.key \
  -p COM3

# With device configuration
python provision_certificates.py \
  --device-name "device001" \
  --zipcode "60607" \
  --user-name "john_doe" \
  -p COM3
```

The Python script will:
- Auto-detect certificates from `certs_to_provision/` directory
- Generate NVS partition binary
- Flash to device automatically

### Method 3: Manual Steps

1. **Create NVS CSV file** (`nvs_certs.csv`):
```csv
key,type,encoding,value
device_cfg,namespace,,
ca_cert,file,binary,/absolute/path/to/ca.crt
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

### Configs CSV Format

Provide device metadata in `configs.csv` located in the working directory:

```csv
name,zipcode,user_name
device001,60607,john_doe
```

The script auto-detects `configs.csv` and will populate the NVS CSV with these values alongside the auto-detected `.crt` and `.key` files.

## Partition Information

From `partitions.csv`:
- **NVS Offset**: 0x9000
- **NVS Size**: 0x6000 (24 KB)

## NVS Keys

The following keys are used in the `device_cfg` namespace:

| Key | Type | Description |
|-----|------|-------------|
| `ca_cert` | blob | CA certificate (PEM format) |
| `client_cert` | blob | Client TLS certificate (PEM format) |
| `client_key` | blob | Client TLS private key (PEM format) |
| `name` | string | Device name (16 chars max) |
| `zipcode` | string | Device zipcode (8 chars max) |
| `user_name` | string | User name (16 chars max) |

## Verification

After provisioning, monitor the serial output during boot:

```
I (1234) DEVICE_CONFIG: Loaded CA certificate from NVS (1150 bytes)
I (1238) DEVICE_CONFIG: Loaded client certificate from NVS (1234 bytes)
I (1240) DEVICE_CONFIG: Loaded client key from NVS (1675 bytes)
I (1250) MQTT: Loaded CA (1150 bytes), client cert (1234 bytes), key (1675 bytes) from NVS
```

## Troubleshooting

### "CA certificate not found in NVS" or "Client certificate not found in NVS"

This error means the NVS partition doesn't contain the required certificates. Solutions:

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

1. Generate device-specific certificate and key (or use existing CA)
2. Run provisioning script:
   ```bash
   ./provision_certificates.sh \
     --ca path/to/ca.crt \
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

// Get CA certificate from NVS
int Device_Config__Get_CA_Cert(char *ca_buffer, size_t buffer_size, size_t *actual_size);

// Get client certificate from NVS
int Device_Config__Get_Client_Cert(char *cert_buffer, size_t buffer_size, size_t *actual_size);

// Get client key from NVS
int Device_Config__Get_Client_Key(char *key_buffer, size_t buffer_size, size_t *actual_size);

// Set device configuration
int Device_Config__Set_Name(const char *name);
int Device_Config__Set_Zipcode(const char *zipcode);
int Device_Config__Set_UserName(const char *user_name);

// Set CA certificate in NVS
int Device_Config__Set_CA_Cert(const char *cert, size_t cert_len);

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
