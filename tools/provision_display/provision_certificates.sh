#!/bin/bash
# Certificate Provisioning Script for ESP32
# Generates NVS partition binary with TLS certificates and flashes to device

set -e

# Default values
CA_FILE="ca.crt"
CERT_FILE="device002.crt"
KEY_FILE="device002.key"
OUTPUT_FILE="nvs_certs.bin"
PORT="/dev/ttyUSB0"
NVS_OFFSET="0x9000"
NVS_SIZE="0x6000"
DEVICE_NAME=""
ZIPCODE=""
USER_NAME=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --ca)
            CA_FILE="$2"
            shift 2
            ;;
        --cert)
            CERT_FILE="$2"
            shift 2
            ;;
        --key)
            KEY_FILE="$2"
            shift 2
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        --device-name)
            DEVICE_NAME="$2"
            shift 2
            ;;
        --zipcode)
            ZIPCODE="$2"
            shift 2
            ;;
        --user-name)
            USER_NAME="$2"
            shift 2
            ;;
        --output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --ca PATH            Path to CA certificate (default: ca.crt)"
            echo "  --cert PATH          Path to client certificate (default: $CERT_FILE)"
            echo "  --key PATH           Path to client key (default: $KEY_FILE)"
            echo "  --port PORT          Serial port (default: $PORT)"
            echo "  --device-name NAME   Device name (8 chars max, optional)"
            echo "  --zipcode ZIP        Zipcode (8 chars max, optional)"
            echo "  --user-name USER     User name (16 chars max, optional)"
            echo "  --output FILE        Output binary file (default: $OUTPUT_FILE)"
            echo "  --help               Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "========================================"
echo "ESP32 Certificate Provisioning Tool"
echo "========================================"
echo ""
# Check if CA certificate file exists
if [ ! -f "$CA_FILE" ]; then
    echo "ERROR: CA certificate file not found: $CA_FILE"
    exit 1
fi
# Check if certificate file exists
if [ ! -f "$CERT_FILE" ]; then
    echo "ERROR: Certificate file not found: $CERT_FILE"
    exit 1
fi

# Check if key file exists
if [ ! -f "$KEY_FILE" ]; then
    echo "ERROR: Key file not found: $KEY_FILE"
    exit 1
fi

# Check if IDF_PATH is set
if [ -z "$IDF_PATH" ]; then
    echo "ERROR: IDF_PATH environment variable not set"
    echo "Please source the ESP-IDF environment:"
    echo "  . \$IDF_PATH/export.sh"
    exit 1
fi

echo "Reading certificate files..."
CA_SIZE=$(stat -f%z "$CA_FILE" 2>/dev/null || stat -c%s "$CA_FILE" 2>/dev/null)
CERT_SIZE=$(stat -f%z "$CERT_FILE" 2>/dev/null || stat -c%s "$CERT_FILE" 2>/dev/null)
KEY_SIZE=$(stat -f%z "$KEY_FILE" 2>/dev/null || stat -c%s "$KEY_FILE" 2>/dev/null)

echo "  CA Certificate: $CA_FILE ($CA_SIZE bytes)"
echo "  Client Certificate: $CERT_FILE ($CERT_SIZE bytes)"
echo "  Client Key: $KEY_FILE ($KEY_SIZE bytes)"
echo ""

# Create CSV file for NVS partition generator
CSV_FILE="nvs_certs.csv"
echo "key,type,encoding,value" > "$CSV_FILE"
echo "device_cfg,namespace,," >> "$CSV_FILE"

# Get absolute paths for certificate files
CA_ABS=$(cd "$(dirname "$CA_FILE")" && pwd)/$(basename "$CA_FILE")
CERT_ABS=$(cd "$(dirname "$CERT_FILE")" && pwd)/$(basename "$CERT_FILE")
KEY_ABS=$(cd "$(dirname "$KEY_FILE")" && pwd)/$(basename "$KEY_FILE")

echo "ca_cert,file,binary,$CA_ABS" >> "$CSV_FILE"
echo "client_cert,file,binary,$CERT_ABS" >> "$CSV_FILE"
echo "client_key,file,binary,$KEY_ABS" >> "$CSV_FILE"

if [ -n "$DEVICE_NAME" ]; then
    echo "name,data,string,$DEVICE_NAME" >> "$CSV_FILE"
fi

if [ -n "$ZIPCODE" ]; then
    echo "zipcode,data,string,$ZIPCODE" >> "$CSV_FILE"
fi

if [ -n "$USER_NAME" ]; then
    echo "user_name,data,string,$USER_NAME" >> "$CSV_FILE"
fi

echo "Created NVS CSV: $CSV_FILE"
echo ""

# Generate NVS partition binary
NVS_GEN_TOOL="$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"

if [ ! -f "$NVS_GEN_TOOL" ]; then
    echo "ERROR: nvs_partition_gen.py not found at: $NVS_GEN_TOOL"
    exit 1
fi

echo "Generating NVS partition binary..."
python "$NVS_GEN_TOOL" generate "$CSV_FILE" "$OUTPUT_FILE" "$NVS_SIZE"

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Failed to generate NVS partition"
    exit 1
fi

echo ""
echo "✓ Successfully generated: $OUTPUT_FILE"
echo ""

# Ask if user wants to flash
read -p "Flash to device on port $PORT? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Flashing NVS partition to device..."
    esptool.py -p "$PORT" write_flash "$NVS_OFFSET" "$OUTPUT_FILE"
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✓ Successfully flashed certificates to device!"
        echo ""
        echo "You can now reset your device. The certificates will be"
        echo "loaded from NVS on the next boot."
    else
        echo ""
        echo "ERROR: Failed to flash to device"
        exit 1
    fi
else
    echo ""
    echo "To flash manually, run:"
    echo "  esptool.py -p $PORT write_flash $NVS_OFFSET $OUTPUT_FILE"
fi

echo ""
echo "========================================"
echo "Provisioning Complete"
echo "========================================"
