#!/bin/bash

# Deploy firmware binary to web server

set -e  # Exit on any error

echo "Deploying firmware..."

# Make binary executable
chmod +x weather_display.bin

# Move to web root
sudo mv weather_display.bin /var/www/html/firmware.bin

echo "Firmware deployed to /var/www/html/firmware.bin"

# Change to server app directory
cd server_app

echo "Changed to server_app directory"
