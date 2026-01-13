@echo off
REM MQTT Publish Wrapper
REM Right-click and run to send MQTT messages with interactive prompts
REM Window stays open to confirm publish result

setlocal enabledelayedexpansion
cd /d "%~dp0.."

powershell -NoExit -ExecutionPolicy Bypass -File "%~dp0mqtt_publish.ps1"
