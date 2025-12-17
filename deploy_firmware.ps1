# PowerShell script to SCP firmware binary to remote server
# Usage: .\deploy_firmware.ps1

# Define paths
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $PSScriptRoot "build"
$firmwareBin = Join-Path $buildDir "weather_display.bin"
$keyFile = Join-Path $projectRoot "id_ed25519"
$remoteUser = "ubuntu"
$remoteHost = "170.9.225.9"
$remotePath = "/tmp/"

Write-Host "=== Firmware Deployment Script ===" -ForegroundColor Cyan
Write-Host "Project root: $projectRoot"
Write-Host "Firmware bin: $firmwareBin"
Write-Host "Key file: $keyFile"
Write-Host ""

# Verify firmware binary exists
if (-not (Test-Path $firmwareBin)) {
    Write-Error "Firmware binary not found at: $firmwareBin"
    Write-Host ""
    Write-Host "Checking for .bin files in build directory:" -ForegroundColor Yellow
    $binFiles = Get-ChildItem (Join-Path $PSScriptRoot "build") -Filter "*.bin" -ErrorAction SilentlyContinue
    if ($binFiles) {
        $binFiles | ForEach-Object { Write-Host "  Found: $($_.Name)" }
    } else {
        Write-Host "  No .bin files found - you may need to build first: idf.py build"
    }
    exit 1
}

# Verify key file exists
if (-not (Test-Path $keyFile)) {
    Write-Error "Key file not found at: $keyFile"
    exit 1
}

# Fix key file permissions for Windows SSH
Write-Host "Checking SSH key permissions..." -ForegroundColor Gray
try {
    # Remove inheritance and set restrictive permissions (owner only)
    icacls $keyFile /inheritance:r /grant:r "${env:USERNAME}:F" /grant:r "SYSTEM:F" /remove "BUILTIN\Users" 2>&1 | Out-Null
    Write-Host "SSH key permissions fixed" -ForegroundColor Green
} catch {
    Write-Warning "Could not auto-fix key permissions, attempting manual fix..."
    Write-Host "Run this command as Administrator:" -ForegroundColor Yellow
    Write-Host "  icacls `"$keyFile`" /inheritance:r /grant:r `"%USERNAME%:F`" /grant:r `"SYSTEM:F`"" -ForegroundColor Cyan
}

# Check if scp is available
$scpCmd = Get-Command scp -ErrorAction SilentlyContinue
if (-not $scpCmd) {
    Write-Error "scp command not found!"
    Write-Host ""
    Write-Host "To fix this:" -ForegroundColor Yellow
    Write-Host "  Option 1: Install OpenSSH for Windows"
    Write-Host "    Run as Admin: Add-WindowsCapability -Online -Name OpenSSH.Client"
    Write-Host ""
    Write-Host "  Option 2: Install Git for Windows (includes scp)"
    Write-Host "    Download from: https://git-scm.com/download/win"
    exit 1
}

Write-Host "Deploying firmware to remote server..." -ForegroundColor Green
Write-Host "  Source: $([System.IO.Path]::GetFileName($firmwareBin))"
Write-Host "  Dest: $remoteUser@$remoteHost`:$remotePath"
Write-Host ""

# Use scp with the key file
try {
    # Check if file is readable
    $fileSize = (Get-Item $firmwareBin).Length
    Write-Host "Firmware size: $([Math]::Round($fileSize/1024, 2)) KB"
    Write-Host ""
    
    $scpCmd = "scp -i `"$keyFile`" `"$firmwareBin`" `"${remoteUser}@${remoteHost}:${remotePath}`""
    Write-Host "Running: $scpCmd" -ForegroundColor Gray
    Write-Host ""
    
    & scp -i $keyFile $firmwareBin "${remoteUser}@${remoteHost}:${remotePath}"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "SUCCESS! Firmware deployed." -ForegroundColor Green
        Write-Host "Remote location: ${remoteUser}@${remoteHost}:${remotePath}weather_display.bin"
    } else {
        Write-Error "SCP transfer failed with exit code: $LASTEXITCODE"
        Write-Host ""
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  1. Check SSH key permissions (should be 600)"
        Write-Host "  2. Verify remote server is reachable: ping $remoteHost"
        Write-Host "  3. Test SSH connection: ssh -i $keyFile $remoteUser@$remoteHost"
        exit 1
    }
}
catch {
    Write-Error "Failed to execute scp: $($_.Exception.Message)"
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  1. Ensure scp command is available"
    Write-Host "  2. Check file paths are correct"
    Write-Host "  3. Verify network connectivity"
    exit 1
}
