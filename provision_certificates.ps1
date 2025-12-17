# Certificate Provisioning Script for ESP32
# This script provisions TLS certificates to the ESP32's NVS storage

param(
    [Parameter(Mandatory=$false)]
    [string]$Port = "COM3",
    
    [Parameter(Mandatory=$false)]
    [string]$CertFile = "main\TLS_Keys\device002.crt",
    
    [Parameter(Mandatory=$false)]
    [string]$KeyFile = "main\TLS_Keys\device002.key",
    
    [Parameter(Mandatory=$false)]
    [string]$DeviceName = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Zipcode = "",
    
    [Parameter(Mandatory=$false)]
    [string]$UserName = ""
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "ESP32 Certificate Provisioning Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if certificate file exists
if (-not (Test-Path $CertFile)) {
    Write-Host "ERROR: Certificate file not found: $CertFile" -ForegroundColor Red
    exit 1
}

# Check if key file exists
if (-not (Test-Path $KeyFile)) {
    Write-Host "ERROR: Key file not found: $KeyFile" -ForegroundColor Red
    exit 1
}

# Read certificate and key files
Write-Host "Reading certificate files..." -ForegroundColor Yellow
$certContent = Get-Content -Path $CertFile -Raw
$keyContent = Get-Content -Path $KeyFile -Raw

Write-Host "  Certificate: $CertFile ($($certContent.Length) bytes)" -ForegroundColor Green
Write-Host "  Key: $KeyFile ($($keyContent.Length) bytes)" -ForegroundColor Green
Write-Host ""

# Create temporary Python script for provisioning
$pythonScript = @"
import sys
import argparse
from esp_idf_nvs_partition_gen import nvs_partition_gen

def provision_certificates(port, cert_file, key_file, device_name=None, zipcode=None):
    """Provision certificates to ESP32 NVS via serial"""
    import serial
    import time
    
    # Read certificate and key files
    with open(cert_file, 'rb') as f:
        cert_data = f.read()
    
    with open(key_file, 'rb') as f:
        key_data = f.read()
    
    print(f"Certificate size: {len(cert_data)} bytes")
    print(f"Key size: {len(key_data)} bytes")
    
    # For now, we'll use esptool.py to write directly to NVS partition
    # This is a placeholder - actual implementation would use NVS partition tools
    print("\\nProvisioning via serial port not yet implemented.")
    print("Please use the HTTP provisioning endpoint instead.")
    print("\\nAlternative: Use parttool.py to write NVS partition")
    
    return 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Provision TLS certificates to ESP32')
    parser.add_argument('--port', required=True, help='Serial port')
    parser.add_argument('--cert', required=True, help='Certificate file')
    parser.add_argument('--key', required=True, help='Key file')
    parser.add_argument('--device-name', help='Device name (optional)')
    parser.add_argument('--zipcode', help='Zipcode (optional)')
    
    args = parser.parse_args()
    sys.exit(provision_certificates(args.port, args.cert, args.key, args.device_name, args.zipcode))
"@

# Save temporary Python script
$tempPyScript = "temp_provision.py"
$pythonScript | Out-File -FilePath $tempPyScript -Encoding UTF8

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Provisioning via HTTP REST API" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Since direct NVS provisioning via serial requires complex" -ForegroundColor Yellow
Write-Host "partition manipulation, we recommend using the HTTP REST API" -ForegroundColor Yellow
Write-Host "provisioning endpoint instead." -ForegroundColor Yellow
Write-Host ""

# Create provisioning code snippet
$httpProvisionCode = @"
// Add this to http_rest.c to create a provisioning endpoint

static esp_err_t provision_certs_handler(httpd_req_t *req) {
    char content[8192];  // Adjust size as needed
    int ret = httpd_req_recv(req, content, sizeof(content));
    
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Parse JSON: {"cert": "...", "key": "..."}
    // (Use cJSON library for parsing)
    
    // Save to NVS
    Device_Config__Set_Client_Cert(cert_data, cert_len);
    Device_Config__Set_Client_Key(key_data, key_len);
    
    httpd_resp_sendstr(req, "Certificates provisioned successfully");
    return ESP_OK;
}
"@

Write-Host "REST API Provisioning Endpoint Code:" -ForegroundColor Cyan
Write-Host $httpProvisionCode -ForegroundColor Gray
Write-Host ""

# Offer to create a curl command for HTTP provisioning
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "HTTP Provisioning Command" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "You can provision certificates via HTTP with:" -ForegroundColor Yellow
Write-Host ""

# Escape certificate and key content for JSON
$certJson = ($certContent -replace '\\', '\\\\' -replace '"', '\"' -replace "`r`n", "\n" -replace "`n", "\n")
$keyJson = ($keyContent -replace '\\', '\\\\' -replace '"', '\"' -replace "`r`n", "\n" -replace "`n", "\n")

$curlCommand = "curl -X POST http://192.168.0.XXX/api/provision -H 'Content-Type: application/json' -d '{\"cert\":\"$certJson\",\"key\":\"$keyJson\"}'"

Write-Host $curlCommand -ForegroundColor Green
Write-Host ""

# Alternative: Generate NVS partition binary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Alternative: NVS Partition Generation" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "You can also generate an NVS partition binary and flash it:" -ForegroundColor Yellow
Write-Host ""

# Create CSV file for NVS partition generation
$nvsCSV = "nvs_certs.csv"

# Convert to absolute paths (required by nvs_partition_gen.py)
$CertFileAbs = (Resolve-Path $CertFile).Path
$KeyFileAbs = (Resolve-Path $KeyFile).Path

$csvContent = @"
key,type,encoding,value
device_cfg,namespace,,
client_cert,file,binary,$CertFileAbs
client_key,file,binary,$KeyFileAbs
"@

if ($DeviceName -ne "") {
    $csvContent += "`nname,data,string,$DeviceName"
}

if ($Zipcode -ne "") {
    $csvContent += "`nzipcode,data,string,$Zipcode"
}

if ($UserName -ne "") {
    $csvContent += "`nuser_name,data,string,$UserName"
}

$csvContent | Out-File -FilePath $nvsCSV -Encoding UTF8

Write-Host "Created NVS CSV file: $nvsCSV" -ForegroundColor Green
Write-Host ""
Write-Host "To generate and flash the NVS partition:" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. Generate NVS binary:" -ForegroundColor Cyan
Write-Host "   python `$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate $nvsCSV nvs_certs.bin 0x6000" -ForegroundColor Green
Write-Host ""
Write-Host "2. Flash to device (find NVS partition offset from partitions.csv):" -ForegroundColor Cyan
Write-Host "   esptool.py -p $Port write_flash 0x9000 nvs_certs.bin" -ForegroundColor Green
Write-Host ""

# Clean up
Remove-Item -Path $tempPyScript -ErrorAction SilentlyContinue

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Provisioning Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "NVS CSV file created: $nvsCSV" -ForegroundColor Green
Write-Host "Follow the instructions above to flash certificates to your device." -ForegroundColor Yellow
