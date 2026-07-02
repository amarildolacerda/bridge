#!/bin/bash
set -e
cd "$(dirname "$0")"

PORT="/dev/ttyUSB0"
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port) PORT="$2"; shift 2 ;;
        -h|--help)
            echo "Uso: $0 [-p <porta>]"
            echo "  -p, --port   Porta serial (default: /dev/ttyUSB0)"
            exit 0 ;;
        *) echo "Opcao desconhecida: $1"; exit 1 ;;
    esac
done

echo "Building and flashing ESP8266 Gateway on ${PORT}..."
pio run -e esp8266_gateway --target upload --upload-port "$PORT"
echo "Flash complete."
