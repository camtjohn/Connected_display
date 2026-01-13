@echo off
REM ESP32 Certificate Provisioning - Quick Launch
REM Double-click this file to provision certificates to your ESP32 device

echo ========================================
echo ESP32 Certificate Provisioning
echo ========================================
echo.
echo This will auto-detect:
echo   - ca.crt (CA certificate)
echo   - device*.crt (client certificate)
echo   - device*.key (client key)
echo.

REM Get the directory where this script is located
cd /d "%~dp0"

REM Run the PowerShell script with default port
powershell.exe -ExecutionPolicy Bypass -File ".\provision_certificates.ps1" -Port COM18

echo.
echo ========================================
echo.
pause
