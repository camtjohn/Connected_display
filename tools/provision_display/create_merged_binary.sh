#!/bin/bash
# Merged Binary Creator for ESP32
# Provisions certificates from certs_to_provision/ and merges with app binary
# Creates a single flashable binary with bootloader, certs, partition table, OTA data, and app

set -e

# Default values
PROJECT_ROOT="${PROJECT_ROOT:-./../..}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
TOOLS_DIR="${TOOLS_DIR:-.}"
OUTPUT_FILE="${OUTPUT_FILE:-$BUILD_DIR/weather_display_merged.bin}"

# Certificate files (for generating NVS binary)
CA_FILE="${CA_FILE:-$TOOLS_DIR/certs_to_provision/ca.crt}"
CERT_FILE="${CERT_FILE:-$TOOLS_DIR/certs_to_provision/device002.crt}"
KEY_FILE="${KEY_FILE:-$TOOLS_DIR/certs_to_provision/device002.key}"

# Default config values
DEVICE_NAME=""
ZIPCODE="60607"
USER_NAME="DefaultUser"

# Flash offsets and sizes from partition table
NVS_OFFSET="0x9000"
NVS_SIZE="0x6000"
PARTITION_TABLE_OFFSET="0x8000"
OTA_DATA_OFFSET="0x10000"
APP_OFFSET="0x110000"

# Expected binary files
BOOTLOADER_BIN="$BUILD_DIR/bootloader/bootloader.bin"
NVS_BIN="$TOOLS_DIR/nvs_certs.bin"
PARTITION_TABLE_BIN="$BUILD_DIR/partition_table/partition-table.bin"
OTA_DATA_BIN="$BUILD_DIR/ota_data_initial.bin"
APP_BIN="$BUILD_DIR/weather_display.bin"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --project-root)
            PROJECT_ROOT="$2"
            BUILD_DIR="$PROJECT_ROOT/build"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
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
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Provisions TLS certificates and creates a merged binary containing:"
            echo "  bootloader, provisioned certificates, partition table, OTA data, and app"
            echo ""
            echo "The merged binary can be flashed in one command."
            echo ""
            echo "Options:"
            echo "  --output FILE           Output merged binary file"
            echo "                          (default: build/weather_display_merged.bin)"
            echo "  --project-root PATH     Project root directory"
            echo "                          (default: ../../..)"
            echo "  --build-dir PATH        Build directory (default: PROJECT_ROOT/build)"
            echo "  --ca FILE               CA certificate file"
            echo "                          (default: ./certs_to_provision/ca.crt)"
            echo "  --cert FILE             Client certificate file"
            echo "                          (default: ./certs_to_provision/device002.crt)"
            echo "  --key FILE              Client key file"
            echo "                          (default: ./certs_to_provision/device002.key)"
            echo "  --device-name NAME      Device name (auto-detected from cert filename)"
            echo "  --zipcode ZIP           Zipcode (default: 60607)"
            echo "  --user-name USER        User name (default: DefaultUser)"
            echo "  --help                  Show this help message"
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
echo "ESP32 Merged Binary Creator"
echo "========================================"
echo ""

# Function to check if file exists
check_file() {
    local file=$1
    local name=$2
    if [ ! -f "$file" ]; then
        echo "ERROR: $name not found: $file"
        exit 1
    fi
}

# Verify all required files exist
echo "Checking required files..."
check_file "$BOOTLOADER_BIN" "Bootloader"
check_file "$PARTITION_TABLE_BIN" "Partition table"
check_file "$OTA_DATA_BIN" "OTA data"
check_file "$APP_BIN" "Application binary"

# Check certificate files for NVS generation
check_file "$CA_FILE" "CA certificate"
check_file "$CERT_FILE" "Client certificate"
check_file "$KEY_FILE" "Client key"
echo "✓ All required files found"
echo ""

# Extract and validate device name from certificate filename
if [ -z "$DEVICE_NAME" ]; then
    # Get just the filename without path and extension
    CERT_BASENAME=$(basename "$CERT_FILE" .crt)
    KEY_BASENAME=$(basename "$KEY_FILE" .key)
    
    # Check if certificate filename matches devXX pattern (dev + exactly 2 digits)
    if [[ ! "$CERT_BASENAME" =~ ^dev[0-9][0-9]$ ]]; then
        echo "ERROR: Client certificate filename must follow pattern 'devXX.crt'"
        echo "       where XX is exactly 2 digits (e.g., dev00.crt, dev01.crt, dev99.crt)"
        echo "       Current filename: $CERT_FILE"
        exit 1
    fi
    
    # Check if key filename matches devXX pattern
    if [[ ! "$KEY_BASENAME" =~ ^dev[0-9][0-9]$ ]]; then
        echo "ERROR: Client key filename must follow pattern 'devXX.key'"
        echo "       where XX is exactly 2 digits (e.g., dev00.key, dev01.key, dev99.key)"
        echo "       Current filename: $KEY_FILE"
        exit 1
    fi
    
    # Check if both filenames match
    if [ "$CERT_BASENAME" != "$KEY_BASENAME" ]; then
        echo "ERROR: Certificate and key filenames must match"
        echo "       Certificate: $CERT_FILE"
        echo "       Key:         $KEY_FILE"
        exit 1
    fi
    
    # Extract device name from filename
    DEVICE_NAME="$CERT_BASENAME"
    echo "✓ Detected device name from certificates: $DEVICE_NAME"
else
    echo "✓ Using provided device name: $DEVICE_NAME"
fi
echo ""

# Generate NVS certificate binary if it doesn't exist or if certificates are newer
if [ ! -f "$NVS_BIN" ] || [ "$CA_FILE" -nt "$NVS_BIN" ] || [ "$CERT_FILE" -nt "$NVS_BIN" ] || [ "$KEY_FILE" -nt "$NVS_BIN" ]; then
    echo "Generating NVS certificate partition..."
    
    # Check if IDF_PATH is set
    if [ -z "$IDF_PATH" ]; then
        echo "ERROR: IDF_PATH environment variable not set"
        echo "Please source the ESP-IDF environment:"
        echo "  . \$IDF_PATH/export.sh"
        exit 1
    fi
    
    NVS_GEN_TOOL="$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"
    
    if [ ! -f "$NVS_GEN_TOOL" ]; then
        echo "ERROR: nvs_partition_gen.py not found at: $NVS_GEN_TOOL"
        exit 1
    fi
    
    # Create temporary CSV file for NVS partition
    CSV_FILE="$TOOLS_DIR/.nvs_temp.csv"
    echo "key,type,encoding,value" > "$CSV_FILE"
    echo "device_cfg,namespace,," >> "$CSV_FILE"
    
    # Get absolute paths for certificate files
    CA_ABS=$(cd "$(dirname "$CA_FILE")" && pwd)/$(basename "$CA_FILE")
    CERT_ABS=$(cd "$(dirname "$CERT_FILE")" && pwd)/$(basename "$CERT_FILE")
    KEY_ABS=$(cd "$(dirname "$KEY_FILE")" && pwd)/$(basename "$KEY_FILE")
    
    echo "ca_cert,file,binary,$CA_ABS" >> "$CSV_FILE"
    echo "client_cert,file,binary,$CERT_ABS" >> "$CSV_FILE"
    echo "client_key,file,binary,$KEY_ABS" >> "$CSV_FILE"
    
    # Add device configuration to NVS
    echo "name,data,string,$DEVICE_NAME" >> "$CSV_FILE"
    echo "zipcode,data,string,$ZIPCODE" >> "$CSV_FILE"
    echo "user_name,data,string,$USER_NAME" >> "$CSV_FILE"
    
    # Generate NVS binary
    python "$NVS_GEN_TOOL" generate "$CSV_FILE" "$NVS_BIN" "$NVS_SIZE"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to generate NVS partition"
        rm -f "$CSV_FILE"
        exit 1
    fi
    
    rm -f "$CSV_FILE"
    echo "✓ Generated NVS certificate partition"
    echo ""
fi

# Get file sizes
BOOTLOADER_FILE_SIZE=$(stat -f%z "$BOOTLOADER_BIN" 2>/dev/null || stat -c%s "$BOOTLOADER_BIN" 2>/dev/null)
NVS_FILE_SIZE=$(stat -f%z "$NVS_BIN" 2>/dev/null || stat -c%s "$NVS_BIN" 2>/dev/null)
PARTITION_TABLE_FILE_SIZE=$(stat -f%z "$PARTITION_TABLE_BIN" 2>/dev/null || stat -c%s "$PARTITION_TABLE_BIN" 2>/dev/null)
OTA_DATA_FILE_SIZE=$(stat -f%z "$OTA_DATA_BIN" 2>/dev/null || stat -c%s "$OTA_DATA_BIN" 2>/dev/null)
APP_FILE_SIZE=$(stat -f%z "$APP_BIN" 2>/dev/null || stat -c%s "$APP_BIN" 2>/dev/null)

echo "Input files:"
echo "  Bootloader:      $BOOTLOADER_BIN"
echo "                   Size: $BOOTLOADER_FILE_SIZE bytes"
echo ""
echo "  CA Certificate:  $CA_FILE"
echo "  Client Cert:     $CERT_FILE"
echo "  Client Key:      $KEY_FILE"
echo ""
echo "  Partition Table: $PARTITION_TABLE_BIN"
echo "                   Size: $PARTITION_TABLE_FILE_SIZE bytes @ 0x$PARTITION_TABLE_OFFSET"
echo ""
echo "  OTA Data:        $OTA_DATA_BIN"
echo "                   Size: $OTA_DATA_FILE_SIZE bytes @ 0x$OTA_DATA_OFFSET"
echo ""
echo "  Application:     $APP_BIN"
echo "                   Size: $APP_FILE_SIZE bytes @ 0x$APP_OFFSET"
echo ""

# Validate file sizes don't exceed allocated space
validate_size() {
    local file_size=$1
    local allocated_size=$2
    local offset=$3
    local name=$4
    
    # Convert hex to decimal for comparison
    allocated_dec=$((allocated_size))
    
    if [ "$file_size" -gt "$allocated_dec" ]; then
        echo "ERROR: $name exceeds allocated space!"
        echo "  File size: $file_size bytes"
        echo "  Allocated: $allocated_dec bytes (@ 0x$(printf '%x' $offset))"
        exit 1
    fi
}

echo "Provisioning certificates..."

# Generate NVS certificate binary if it doesn't exist or if certificates are newer
if [ ! -f "$NVS_BIN" ] || [ "$CA_FILE" -nt "$NVS_BIN" ] || [ "$CERT_FILE" -nt "$NVS_BIN" ] || [ "$KEY_FILE" -nt "$NVS_BIN" ]; then
    echo "  Generating NVS certificate partition from:"
    echo "    - $CA_FILE"
    echo "    - $CERT_FILE"
    echo "    - $KEY_FILE"
    
    # Check if IDF_PATH is set
    if [ -z "$IDF_PATH" ]; then
        echo "ERROR: IDF_PATH environment variable not set"
        echo "Please source the ESP-IDF environment:"
        echo "  . \$IDF_PATH/export.sh"
        exit 1
    fi
    
    NVS_GEN_TOOL="$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"
    
    if [ ! -f "$NVS_GEN_TOOL" ]; then
        echo "ERROR: nvs_partition_gen.py not found at: $NVS_GEN_TOOL"
        exit 1
    fi
    
    # Create temporary CSV file for NVS partition
    CSV_FILE="$TOOLS_DIR/.nvs_temp.csv"
    echo "key,type,encoding,value" > "$CSV_FILE"
    echo "device_cfg,namespace,," >> "$CSV_FILE"
    
    # Get absolute paths for certificate files
    CA_ABS=$(cd "$(dirname "$CA_FILE")" && pwd)/$(basename "$CA_FILE")
    CERT_ABS=$(cd "$(dirname "$CERT_FILE")" && pwd)/$(basename "$CERT_FILE")
    KEY_ABS=$(cd "$(dirname "$KEY_FILE")" && pwd)/$(basename "$KEY_FILE")
    
    echo "ca_cert,file,binary,$CA_ABS" >> "$CSV_FILE"
    echo "client_cert,file,binary,$CERT_ABS" >> "$CSV_FILE"
    echo "client_key,file,binary,$KEY_ABS" >> "$CSV_FILE"
    
    # Add device configuration to NVS
    echo "name,data,string,$DEVICE_NAME" >> "$CSV_FILE"
    echo "zipcode,data,string,$ZIPCODE" >> "$CSV_FILE"
    echo "user_name,data,string,$USER_NAME" >> "$CSV_FILE"
    
    # Generate NVS binary
    python "$NVS_GEN_TOOL" generate "$CSV_FILE" "$NVS_BIN" "$NVS_SIZE"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to generate NVS partition"
        rm -f "$CSV_FILE"
        exit 1
    fi
    
    rm -f "$CSV_FILE"
    echo "  ✓ NVS certificate partition created: $NVS_BIN"
else
    echo "  ✓ Using existing NVS certificate partition: $NVS_BIN"
fi

echo ""

echo "Creating merged binary at: $OUTPUT_FILE"
echo ""

# Create temporary directory
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Create intermediate file with all components at correct offsets
TEMP_BIN="$TEMP_DIR/temp_merged.bin"

# Start with bootloader (at 0x0)
cp "$BOOTLOADER_BIN" "$TEMP_BIN"

# Function to pad file to specific size
pad_file() {
    local file=$1
    local target_size=$2
    local current_size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
    local pad_size=$((target_size - current_size))
    
    if [ "$pad_size" -gt 0 ]; then
        dd if=/dev/zero bs=1 count=$pad_size 2>/dev/null >> "$file"
    fi
}

# Pad bootloader section to NVS offset
echo "Padding bootloader section to NVS offset (0x$NVS_OFFSET)..."
pad_file "$TEMP_BIN" "$((NVS_OFFSET))"

# Append NVS certificate binary
echo "Appending NVS certificate partition (at 0x$NVS_OFFSET)..."
dd if="$NVS_BIN" of="$TEMP_BIN" bs=1 seek=$((NVS_OFFSET)) 2>/dev/null

# Pad to partition table offset
echo "Padding to partition table offset (0x$PARTITION_TABLE_OFFSET)..."
CURRENT_SIZE=$(stat -f%z "$TEMP_BIN" 2>/dev/null || stat -c%s "$TEMP_BIN" 2>/dev/null)
if [ "$CURRENT_SIZE" -lt "$((PARTITION_TABLE_OFFSET))" ]; then
    DIFF=$((PARTITION_TABLE_OFFSET - CURRENT_SIZE))
    dd if=/dev/zero bs=1 count=$DIFF 2>/dev/null >> "$TEMP_BIN"
fi

# Append partition table
echo "Appending partition table (at 0x$PARTITION_TABLE_OFFSET)..."
dd if="$PARTITION_TABLE_BIN" of="$TEMP_BIN" bs=1 seek=$((PARTITION_TABLE_OFFSET)) 2>/dev/null

# Pad to OTA data offset
echo "Padding to OTA data offset (0x$OTA_DATA_OFFSET)..."
CURRENT_SIZE=$(stat -f%z "$TEMP_BIN" 2>/dev/null || stat -c%s "$TEMP_BIN" 2>/dev/null)
if [ "$CURRENT_SIZE" -lt "$((OTA_DATA_OFFSET))" ]; then
    DIFF=$((OTA_DATA_OFFSET - CURRENT_SIZE))
    dd if=/dev/zero bs=1 count=$DIFF 2>/dev/null >> "$TEMP_BIN"
fi

# Append OTA data
echo "Appending OTA data (at 0x$OTA_DATA_OFFSET)..."
dd if="$OTA_DATA_BIN" of="$TEMP_BIN" bs=1 seek=$((OTA_DATA_OFFSET)) 2>/dev/null

# Pad to app offset
echo "Padding to app offset (0x$APP_OFFSET)..."
CURRENT_SIZE=$(stat -f%z "$TEMP_BIN" 2>/dev/null || stat -c%s "$TEMP_BIN" 2>/dev/null)
if [ "$CURRENT_SIZE" -lt "$((APP_OFFSET))" ]; then
    DIFF=$((APP_OFFSET - CURRENT_SIZE))
    dd if=/dev/zero bs=1 count=$DIFF 2>/dev/null >> "$TEMP_BIN"
fi

# Append app binary
echo "Appending application binary (at 0x$APP_OFFSET)..."
dd if="$APP_BIN" of="$TEMP_BIN" bs=1 seek=$((APP_OFFSET)) 2>/dev/null

# Move final merged binary to output location
mv "$TEMP_BIN" "$OUTPUT_FILE"

MERGED_SIZE=$(stat -f%z "$OUTPUT_FILE" 2>/dev/null || stat -c%s "$OUTPUT_FILE" 2>/dev/null)

echo ""
echo "========================================"
echo "✓ Merged binary created successfully!"
echo "========================================"
echo ""
echo "Output file: $OUTPUT_FILE"
echo "Total size:  $MERGED_SIZE bytes ($(numfmt --to=iec $MERGED_SIZE 2>/dev/null || echo "~$((MERGED_SIZE / 1024)) KB"))"
echo ""
echo "To flash to your ESP32, run:"
echo ""
echo "  esptool.py -p /dev/ttyUSB0 write_flash 0x0 $OUTPUT_FILE"
echo ""
echo "Or with additional parameters:"
echo ""
echo "  esptool.py -p /dev/ttyUSB0 -b 460800 --chip esp32s3 --mode dio --freq 80m --flash_size 8MB write_flash 0x0 $OUTPUT_FILE"
echo ""
echo "========================================"

