@echo off
setlocal

echo ========================================
echo Package ESP32 Flash Files
echo ========================================
echo.

set BUILD_DIR=..\..\build
set PACKAGE_DIR=flash_package\support_files
set NVS_CERTS=nvs_certs.bin

rem Check if build files exist
if not exist "%BUILD_DIR%\bootloader\bootloader.bin" (
    echo ERROR: Bootloader not found. Build the project first.
    exit /b 1
)

if not exist "%BUILD_DIR%\partition_table\partition-table.bin" (
    echo ERROR: Partition table not found. Build the project first.
    exit /b 1
)

if not exist "%NVS_CERTS%" (
    echo ERROR: nvs_certs.bin not found. Run provision_certificates.ps1 first.
    exit /b 1
)

if not exist "%BUILD_DIR%\weather_display.bin" (
    echo ERROR: App binary not found. Build the project first.
    exit /b 1
)

if not exist "%BUILD_DIR%\ota_data_initial.bin" (
    echo ERROR: OTA data not found. Build the project first.
    exit /b 1
)

rem Create support_files directory if it doesn't exist
if not exist "flash_package\support_files" mkdir "flash_package\support_files"

rem Copy all required files to flash_package\support_files
echo Copying files to %PACKAGE_DIR%...
copy /Y "%BUILD_DIR%\bootloader\bootloader.bin" "%PACKAGE_DIR%\bootloader.bin"
copy /Y "%BUILD_DIR%\partition_table\partition-table.bin" "%PACKAGE_DIR%\partition-table.bin"
copy /Y "%BUILD_DIR%\ota_data_initial.bin" "%PACKAGE_DIR%\ota_data_initial.bin"
copy /Y "%NVS_CERTS%" "%PACKAGE_DIR%\nvs_certs.bin"
copy /Y "%BUILD_DIR%\weather_display.bin" "%PACKAGE_DIR%\weather_display.bin"

rem Move esptool and README to support_files if they're in flash_package root
if exist "flash_package\esptool.exe" (
    move /Y "flash_package\esptool.exe" "%PACKAGE_DIR%\esptool.exe"
)
if exist "flash_package\README.md" (
    move /Y "flash_package\README.md" "%PACKAGE_DIR%\README.md"
)

echo.
echo ========================================
echo Package ready!
echo ========================================
echo.
echo All files copied to: %PACKAGE_DIR%
echo   - bootloader.bin
echo   - partition-table.bin
echo   - ota_data_initial.bin
echo   - nvs_certs.bin
echo   - weather_display.bin
echo.
echo Files are in flash_package\support_files
echo Flash tools (flash.bat, detect_ports.bat) remain in flash_package root
echo.
echo To flash, copy the entire flash_package folder and run:
echo   cd flash_package
echo   flash.bat [COM_PORT]
echo.

endlocal
