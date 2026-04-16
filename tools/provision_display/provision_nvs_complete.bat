@echo off
REM Generate NVS binary with Certificates + WiFi + Device Settings
REM This script creates nvs_certs.bin with everything included

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0

echo ========================================
echo NVS Complete Provisioning
echo ========================================
echo.
echo This will create an nvs_certs.bin file containing:
echo   - CA Certificate
echo   - Client Certificate  
echo   - Client Key
echo   - WiFi Credentials
echo   - Device Settings (Zipcode, User Name)
echo.

REM Run PowerShell script
powershell.exe -ExecutionPolicy Bypass -File "!SCRIPT_DIR!provision_nvs_complete.ps1"

if errorlevel 1 (
    echo.
    echo ERROR: Failed to generate NVS binary
    pause
    exit /b 1
)

echo.
echo ========================================
echo Next steps:
echo 1. Copy the generated nvs_certs.bin to:
echo    flash_package/support_files/
echo 2. Run flash_package/flash.bat to flash the device
echo ========================================
echo.

pause
