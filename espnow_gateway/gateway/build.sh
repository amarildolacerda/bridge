#!/bin/bash
set -e

cd "$(dirname "$0")"
echo "Building ESP8266 Gateway..."
pio run -e esp8266_gateway
echo "Build complete. Firmware in .pio/build/esp8266_gateway/firmware.bin"