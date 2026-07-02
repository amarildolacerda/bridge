#!/bin/bash
cd "$(dirname "$0")"
pio device monitor -e esp8266_gateway --baud 115200