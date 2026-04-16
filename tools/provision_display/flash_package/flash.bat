@echo off
setlocal enabledelayedexpansion

rem Flash ESP32 with bootloader, NVS certs, partition table, and app at correct offsets
rem Usage: flash.bat [COMPORT]
rem
rem OPTIONAL: To pre-provision WiFi credentials, zipcode, and user name:
rem   1. Edit provision_nvs.bat and update the WIFI_SSID, WIFI_PASSWORD, ZIPCODE, USER_NAME
rem   2. Run provision_nvs.bat to generate the updated nvs_certs.bin
rem   3. Run flash.bat to flash the device with the new provisioning data

rem Get the directory where this batch file is located
set SCRIPT_DIR=%~dp0

set PORT=%1
if "%PORT%"=="" (
    echo.
    echo ESP32 Flash Tool
    echo ================
    echo.
    set PORT=3
    echo Using default port COM%PORT%
    
    if "!PORT!"=="" (
        echo ERROR: COM port required
        pause
        exit /b 1
    )
)

rem Ensure port has COM prefix
if not "!PORT:~0,3!"=="COM" (
    set PORT=COM!PORT!
)

rem Set paths - all binaries must be in support_files directory
setlocal enabledelayedexpansion
set "SUPPORT_FILES_DIR=%SCRIPT_DIR%support_files"

set BOOTLOADER=%SUPPORT_FILES_DIR%\bootloader.bin
set PARTITION_TABLE=%SUPPORT_FILES_DIR%\partition-table.bin
set OTA_DATA=%SUPPORT_FILES_DIR%\ota_data_initial.bin
set NVS_CERTS=%SUPPORT_FILES_DIR%\nvs_certs.bin
set APP_BIN=%SUPPORT_FILES_DIR%\weather_display.bin

echo Checking for required binaries...
echo.

rem Check required files exist
if not exist "!BOOTLOADER!" (
    echo ERROR: Bootloader not found
    echo Expected location: !BOOTLOADER!
    set MISSING_FILE=1
)

if not exist "!PARTITION_TABLE!" (
    echo ERROR: Partition table not found
    echo Expected location: !PARTITION_TABLE!
    set MISSING_FILE=1
)

if not exist "!OTA_DATA!" (
    echo ERROR: OTA data not found
    echo Expected location: !OTA_DATA!
    set MISSING_FILE=1
)

if not exist "!NVS_CERTS!" (
    echo ERROR: NVS certs not found
    echo Expected location: !NVS_CERTS!
    set MISSING_FILE=1
)

if not exist "!APP_BIN!" (
    echo ERROR: App binary not found
    echo Expected location: !APP_BIN!
    set MISSING_FILE=1
)

if defined MISSING_FILE (
    echo.
    echo PACKAGE ERROR: This package is incomplete. All of the following files must exist:
    echo   - bootloader.bin
    echo   - partition-table.bin
    echo   - ota_data_initial.bin
    echo   - nvs_certs.bin
    echo   - weather_display.bin
    echo.
    echo All files should be in: !SUPPORT_FILES_DIR!\
    pause
    exit /b 1
)

rem Use bundled esptool.exe - look in support_files first (packaged), then current directory (backward compat)
if exist "%SCRIPT_DIR%support_files\esptool.exe" (
    set ESPTOOL=%SCRIPT_DIR%support_files\esptool.exe
) else (
    set ESPTOOL=%SCRIPT_DIR%esptool.exe
)

if not exist "%ESPTOOL%" (
    echo ERROR: esptool.exe not found
    pause
    exit /b 1
)

echo.
echo Flashing ESP32 to %PORT% ...
echo   Bootloader:      %BOOTLOADER% @ 0x0
echo   Partition table: %PARTITION_TABLE% @ 0x8000
echo   NVS certs:       %NVS_CERTS% @ 0x9000
echo   OTA data:        %OTA_DATA% @ 0x10000
echo   Application:     %APP_BIN% @ 0x110000
echo.

rem Flash all components at their correct offsets (matching ESP-IDF flash_project_args)
"%ESPTOOL%" --chip esp32s3 -p %PORT% -b 460800 --before default_reset --after hard_reset ^
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB -z ^
  0x0 "%BOOTLOADER%" ^
  0x8000 "%PARTITION_TABLE%" ^
  0x9000 "%NVS_CERTS%" ^
  0x10000 "%OTA_DATA%" ^
  0x110000 "%APP_BIN%"

if errorlevel 1 (
    echo.
    echo Flash failed. Check port and connection, then retry.
    echo.
    pause
    exit /b 1
) else (
    echo.
    echo Flash successful! You can reset the device now.
    echo.
)

echo Press any key to close this terminal...
pause >nul

endlocal
