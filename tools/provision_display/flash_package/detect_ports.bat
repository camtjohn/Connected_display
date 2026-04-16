@echo off
echo.
echo Available Serial Ports:
echo ======================
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-CimInstance Win32_SerialPort | ForEach-Object { Write-Host \"$($_.DeviceID) - $($_.Description)\" }"
echo.
echo Note the COM port number for your ESP32 device (typically USB port)
echo.
pause
