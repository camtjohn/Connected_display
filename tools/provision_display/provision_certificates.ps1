param(
    [Parameter(Mandatory=$false)]
    [string]$Port = "COM18",
    
    [Parameter(Mandatory=$false)]
    [string]$CaFile = "",
    
    [Parameter(Mandatory=$false)]
    [string]$CertFile = "",
    
    [Parameter(Mandatory=$false)]
    [string]$KeyFile = "",
    
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

# Auto-detect .crt in certs_to_provision
if (-not $CertFile -or -not (Test-Path $CertFile)) {
    $crtFiles = Get-ChildItem -Path $certsDir -Filter "device*.crt" -File -ErrorAction SilentlyContinue
    if ($crtFiles.Count -eq 1) {
        $CertFile = $crtFiles[0].FullName
        Write-Host "Auto-detected certificate file: $CertFile" -ForegroundColor Green
    } elseif ($crtFiles.Count -gt 1) {
        Write-Host "ERROR: Multiple device*.crt files found. Use -CertFile to specify which one." -ForegroundColor Red
        exit 1
    } else {
        Write-Host "ERROR: No device*.crt file found in certs_to_provision. Use -CertFile to specify." -ForegroundColor Red
        exit 1
    }
}

# Auto-detect .key in certs_to_provision
if (-not $KeyFile -or -not (Test-Path $KeyFile)) {
    $keyFiles = Get-ChildItem -Path $certsDir -Filter "device*.key" -File -ErrorAction SilentlyContinue
    if ($keyFiles.Count -eq 1) {
        $KeyFile = $keyFiles[0].FullName
        Write-Host "Auto-detected key file: $KeyFile" -ForegroundColor Green
    } elseif ($keyFiles.Count -gt 1) {
        Write-Host "ERROR: Multiple device*.key files found. Use -KeyFile to specify which one." -ForegroundColor Red
        exit 1
    } else {
        Write-Host "ERROR: No device*.key file found in certs_to_provision. Use -KeyFile to specify." -ForegroundColor Red
        exit 1
    }
}

# Read certificate and key files (silent)
$certContent = Get-Content -Path $CertFile -Raw
$keyContent = Get-Content -Path $KeyFile -Raw

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Generating NVS Partition" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
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
"@

# Load configs.csv (name, zipcode, user_name) if present in current directory
$configsPath = Join-Path (Get-Location) "configs.csv"
if (Test-Path $configsPath) {
    try {
        $cfg = Import-Csv -Path $configsPath
        if ($cfg.Count -ge 1) {
            if ($cfg[0].name) { $DeviceName = $cfg[0].name }
            if ($cfg[0].zipcode) { $Zipcode = $cfg[0].zipcode }
            if ($cfg[0].user_name) { $UserName = $cfg[0].user_name }
            Write-Host "Loaded configs from configs.csv" -ForegroundColor Green
        }
    } catch {
        Write-Host "Warning: Failed to parse configs.csv: $($_.Exception.Message)" -ForegroundColor Yellow
    }
} else {
    # Create a template configs.csv if missing
    $template = "name,zipcode,user_name`n,,"
    try {
        $template | Out-File -FilePath $configsPath -Encoding UTF8 -NoClobber
        Write-Host "Created template configs.csv; edit values as needed." -ForegroundColor Yellow
    } catch {}
}

if ($DeviceName -ne "") { $csvContent += "`nname,data,string,$DeviceName" }
if ($Zipcode -ne "")    { $csvContent += "`nzipcode,data,string,$Zipcode" }
if ($UserName -ne "")   { $csvContent += "`nuser_name,data,string,$UserName" }

$csvContent | Out-File -FilePath $nvsCSV -Encoding UTF8

Write-Host "Created NVS CSV file: $nvsCSV" -ForegroundColor Green

# Call the Python script to generate and flash
Write-Host ""
Write-Host "Calling Python provisioning script..." -ForegroundColor Yellow
Write-Host ""

# Define output binary name
$nvsBin = "nvs_certs.bin"

# Build Python command with arguments
$pythonScript = Join-Path (Get-Location) "provision_certificates.py"
$pythonCmd = @(
    $pythonScript,
    "--ca", $CaFile,
    "--cert", $CertFile,
    "--key", $KeyFile,
    "--output", $nvsBin,
    "--port", $Port
)

if ($DeviceName -ne "") { $pythonCmd += @("--device-name", $DeviceName) }
if ($Zipcode -ne "")    { $pythonCmd += @("--zipcode", $Zipcode) }
if ($UserName -ne "")   { $pythonCmd += @("--user-name", $UserName) }

# Run Python script
& python.exe $pythonCmd
