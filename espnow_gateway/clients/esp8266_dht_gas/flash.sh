#!/bin/bash
set -e
command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }
PORT_ARG=""
if [ -n "$1" ]; then
    PORT_ARG="--upload-port $1"
fi
pio run --target upload $PORT_ARG
