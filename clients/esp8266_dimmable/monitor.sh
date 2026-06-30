#!/bin/bash
set -e
cd "$(dirname "$0")"
PORT=${1:-/dev/ttyUSB0}
pio device monitor -p "$PORT" -b 115200