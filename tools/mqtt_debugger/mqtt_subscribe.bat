@echo off
REM MQTT Subscribe Wrapper
REM Right-click and run to listen to MQTT messages
REM Window stays open to show incoming messages

setlocal enabledelayedexpansion
cd /d "%~dp0.."

powershell -NoExit -ExecutionPolicy Bypass -File "%~dp0mqtt_subscribe.ps1"
