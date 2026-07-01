#!/bin/bash
cd "$(dirname "$0")"
pio device monitor -e esp8266_sensor --baud 115200