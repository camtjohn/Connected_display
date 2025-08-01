# Weather Machine 16x16 LED Display

This project is an ESP32-based weather display system using a 16x16 LED matrix. It fetches weather data via MQTT, displays animations, and provides a local UI for configuration.

## Features

- 16x16 LED matrix weather visualization
- WiFi connectivity and configuration
- MQTT support for real-time weather updates
- Local time synchronization
- Modular codebase with FreeRTOS tasks

## Repository Structure

- `main/`  
  Core application source code, including:
  - `main.c` – Main entry point
  - `led_driver.c` – LED matrix control
  - `animation.c` – Display animations
  - `http_rest.c` – HTTP REST client
  - `mqtt.c` – MQTT client
  - `ui.c`, `view.c` – User interface and view logic
  - `spi.c`, `sprite.c` – SPI and sprite handling
  - `tasks.c` – FreeRTOS task management
  - `wifi.c` – WiFi setup and management
  - `local_time.c` – Time synchronization
  - `main.h` – Shared definitions

- `build/`  
  Build artifacts generated by CMake and ESP-IDF.

- `.vscode/`  
  VSCode configuration files.

- `CMakeLists.txt`, `main/CMakeLists.txt`  
  Build configuration using CMake and ESP-IDF.

- `sdkconfig`, `sdkconfig.*`  
  ESP-IDF configuration files.

## Getting Started

1. **Install ESP-IDF**  
   Follow the [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

2. **Configure the Project**  
   Edit `sdkconfig` as needed for your hardware.

3. **Build and Flash and Monitor**  
   idf.py build
   idf.py flash
   idf.py monitor

## License
See [LICENSE](LICENSE) for details.

## Author
Created by Cameron J.
