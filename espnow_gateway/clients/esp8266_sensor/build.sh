#!/bin/bash
set -e

cd "$(dirname "$0")"
echo "Building ESP8266 Sensor..."
pio run -e esp8266_sensor
echo "Build complete. Firmware in .pio/build/esp8266_sensor/firmware.bin"