param(
    [Parameter(Mandatory=$false)]
    [string]$CaFile = "",
    
    [Parameter(Mandatory=$false)]
    [string]$CertFile = "",
    
    [Parameter(Mandatory=$false)]
    [string]$KeyFile = "",
    
    [Parameter(Mandatory=$false)]
    [string]$WiFiSSID = "",
    
    [Parameter(Mandatory=$false)]
    [string]$WiFiPassword = "",
    
    [Parameter(Mandatory=$false)]
    [string]$DeviceName = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Zipcode = "",
    
    [Parameter(Mandatory=$false)]
    [string]$UserName = ""
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "NVS Complete Provisioning Tool" -ForegroundColor Cyan
Write-Host "(Certificates + WiFi + Device Settings)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Try to find and export ESP-IDF environment
$idfPath = $env:IDF_PATH
if (-not $idfPath) {
    # Try common default location
    $username = $env:USERNAME
    $defaultPaths = @(
        "C:\Users\$username\esp\v5.5.1\esp-idf",
        "C:\Users\$username\esp\esp-idf",
        "C:\esp-idf"
    )
    
    foreach ($path in $defaultPaths) {
        if (Test-Path $path) {
            $idfPath = $path
            $env:IDF_PATH = $path
            Write-Host "Found ESP-IDF at: $idfPath" -ForegroundColor Green
            break
        }
    }
}

if ($idfPath -and (Test-Path "$idfPath\export.ps1")) {
    Write-Host "Exporting ESP-IDF environment..." -ForegroundColor Yellow
    & "$idfPath\export.ps1"
    Write-Host "ESP-IDF environment exported" -ForegroundColor Green
    Write-Host ""
}

# Look in certs_to_provision subdirectory
$certsDir = Join-Path (Get-Location) "certs_to_provision"
if (-not (Test-Path $certsDir)) {
    Write-Host "ERROR: Directory 'certs_to_provision' not found." -ForegroundColor Red
    exit 1
}

# Auto-detect ca.crt in certs_to_provision
if (-not $CaFile -or -not (Test-Path $CaFile)) {
    $caPath = Join-Path $certsDir "ca.crt"
    if (Test-Path $caPath) {
        $CaFile = $caPath
        Write-Host "Auto-detected CA certificate: $CaFile" -ForegroundColor Green
    } else {
        Write-Host "ERROR: CA certificate 'ca.crt' not found in certs_to_provision. Use -CaFile to specify." -ForegroundColor Red
        exit 1
    }
}

# Auto-detect .crt in certs_to_provision (dev##.crt pattern where ## is two digits)
if (-not $CertFile -or -not (Test-Path $CertFile)) {
    $crtFiles = Get-ChildItem -Path $certsDir -Filter "dev*.crt" -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^dev\d{2}\.crt$' }
    if ($crtFiles.Count -eq 1) {
        $CertFile = $crtFiles[0].FullName
        Write-Host "Auto-detected certificate file: $CertFile" -ForegroundColor Green
    } elseif ($crtFiles.Count -gt 1) {
        Write-Host "ERROR: Multiple dev##.crt files found. Use -CertFile to specify which one." -ForegroundColor Red
        exit 1
    } else {
        Write-Host "ERROR: No dev##.crt file found in certs_to_provision. Use -CertFile to specify." -ForegroundColor Red
        exit 1
    }
}

# Auto-detect .key in certs_to_provision (dev##.key pattern where ## is two digits)
if (-not $KeyFile -or -not (Test-Path $KeyFile)) {
    $keyFiles = Get-ChildItem -Path $certsDir -Filter "dev*.key" -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^dev\d{2}\.key$' }
    if ($keyFiles.Count -eq 1) {
        $KeyFile = $keyFiles[0].FullName
        Write-Host "Auto-detected key file: $KeyFile" -ForegroundColor Green
    } elseif ($keyFiles.Count -gt 1) {
        Write-Host "ERROR: Multiple dev##.key files found. Use -KeyFile to specify which one." -ForegroundColor Red
        exit 1
    } else {
        Write-Host "ERROR: No dev##.key file found in certs_to_provision. Use -KeyFile to specify." -ForegroundColor Red
        exit 1
    }
}

# Prompt for WiFi credentials if not provided
if (-not $WiFiSSID -or $WiFiSSID -eq "") {
    Write-Host ""
    $WiFiSSID = Read-Host "Enter WiFi SSID"
    if (-not $WiFiSSID -or $WiFiSSID -eq "") {
        Write-Host "ERROR: WiFi SSID is required." -ForegroundColor Red
        exit 1
    }
}

if (-not $WiFiPassword -or $WiFiPassword -eq "") {
    Write-Host ""
    $securePass = Read-Host "Enter WiFi Password" -AsSecureString
    $WiFiPassword = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto([System.Runtime.InteropServices.Marshal]::SecureStringToCoTaskMemUnicode($securePass))
    if (-not $WiFiPassword -or $WiFiPassword -eq "") {
        Write-Host "ERROR: WiFi Password is required." -ForegroundColor Red
        exit 1
    }
}

# Read certificate and key files (silent)
$certContent = Get-Content -Path $CertFile -Raw
$keyContent = Get-Content -Path $KeyFile -Raw

Write-Host ""
Write_Host "========================================" -ForegroundColor Cyan
Write-Host "NVS Configuration" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "WiFi SSID:      $WiFiSSID" -ForegroundColor Yellow
Write-Host "WiFi Password:  $(('*' * $WiFiPassword.Length))" -ForegroundColor Yellow

# Initialize device settings to empty (will be overridden by configs.csv)
$DeviceName = ""
$Zipcode = ""
$UserName = ""

# Load configs.csv (name, zipcode, user_name) if present in current directory
$configsPath = Join-Path (Get-Location) "configs.csv"
if (Test-Path $configsPath) {
    try {
        $cfg = Import-Csv -Path $configsPath
        if ($cfg.Count -ge 1) {
            # Trim whitespace from CSV values
            if ($cfg[0].name) { $DeviceName = $cfg[0].name.Trim() }
            if ($cfg[0].zipcode) { $Zipcode = $cfg[0].zipcode.Trim() }
            if ($cfg[0].user_name) { $UserName = $cfg[0].user_name.Trim() }
            Write-Host "Loaded configs from configs.csv" -ForegroundColor Green
        }
    } catch {
        Write-Host "Warning: Failed to parse configs.csv: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

if ($DeviceName -ne "") { Write-Host "Device Name:    $DeviceName" -ForegroundColor Yellow }
if ($Zipcode -ne "")    { Write-Host "Zipcode:        $Zipcode" -ForegroundColor Yellow }
if ($UserName -ne "")   { Write-Host "User Name:      $UserName" -ForegroundColor Yellow }

Write-Host "CA Cert:        $CaFile" -ForegroundColor Yellow
Write-Host "Device Cert:    $CertFile" -ForegroundColor Yellow
Write-Host "Device Key:     $KeyFile" -ForegroundColor Yellow
Write-Host ""

# Create CSV file for NVS partition generation
$nvsCSV = "nvs_certs.csv"

# Convert to absolute paths (required by nvs_partition_gen.py)
$CaFileAbs = (Resolve-Path $CaFile).Path
$CertFileAbs = (Resolve-Path $CertFile).Path
$KeyFileAbs = (Resolve-Path $KeyFile).Path

$csvContent = @"
key,type,encoding,value
device_cfg,namespace,,
ca_cert,file,binary,$CaFileAbs
client_cert,file,binary,$CertFileAbs
client_key,file,binary,$KeyFileAbs
ssid,data,string,$WiFiSSID
password,data,string,$WiFiPassword
"@

if ($DeviceName -ne "") { $csvContent += "`nname,data,string,$DeviceName" }
if ($Zipcode -ne "")    { $csvContent += "`nzipcode,data,string,$Zipcode" }
if ($UserName -ne "")   { $csvContent += "`nuser_name,data,string,$UserName" }

$csvContent | Out-File -FilePath $nvsCSV -Encoding UTF8

Write-Host "Created NVS CSV file: $nvsCSV" -ForegroundColor Green
Write-Host ""

# Call the Python script to generate NVS binary only (no flashing)
Write-Host "Generating NVS binary..." -ForegroundColor Yellow
Write-Host ""

# Use the existing provision_certificates.py wrapper script with --no-flash option
$pythonScript = Join-Path (Get-Location) "provision_certificates.py"
$pythonCmd = @(
    $pythonScript,
    "--ca", $CaFile,
    "--cert", $CertFile,
    "--key", $KeyFile,
    "--output", "nvs_certs.bin",
    "--no-flash"
)

if ($WiFiSSID -ne "") { $pythonCmd += @("--ssid", $WiFiSSID) }
if ($WiFiPassword -ne "") { $pythonCmd += @("--password", $WiFiPassword) }
if ($DeviceName -ne "") { $pythonCmd += @("--device-name", $DeviceName) }
if ($Zipcode -ne "")    { $pythonCmd += @("--zipcode", $Zipcode) }
if ($UserName -ne "")   { $pythonCmd += @("--user-name", $UserName) }

Write-Host "Running provision_certificates.py..." -ForegroundColor Yellow
& python.exe $pythonCmd

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "SUCCESS!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "NVS binary created: nvs_certs.bin" -ForegroundColor Green
    Write-Host ""
    Write-Host "This file contains:" -ForegroundColor Green
    Write-Host "  - CA Certificate" -ForegroundColor Green
    Write-Host "  - Client Certificate" -ForegroundColor Green
    Write-Host "  - Client Key" -ForegroundColor Green
    Write-Host "  - WiFi SSID: $WiFiSSID" -ForegroundColor Green
    Write-Host "  - Device Name: $DeviceName" -ForegroundColor Green
    Write-Host "  - Zipcode: $Zipcode" -ForegroundColor Green
    Write-Host "  - User Name: $UserName" -ForegroundColor Green
    Write-Host ""
    Write-Host "Copy nvs_certs.bin to your flash_package/support_files/ directory" -ForegroundColor Yellow
    Write-Host "Then run flash.bat to flash the device." -ForegroundColor Yellow
    Write-Host ""
} else {
    Write-Host "ERROR: Failed to generate NVS binary" -ForegroundColor Red
    exit 1
}
