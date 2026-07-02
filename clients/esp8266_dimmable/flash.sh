#!/bin/bash
set -e
cd "$(dirname "$0")"
PORT=${1:-/dev/ttyUSB0}
pio run -e esp8266 -t upload --upload-port "$PORT"