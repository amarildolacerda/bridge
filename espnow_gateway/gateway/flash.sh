#!/bin/bash
set -e
cd "$(dirname "$0")"

PORT="/dev/ttyUSB0"
MODE="serial"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port) PORT="$2"; MODE="serial"; shift 2 ;;
        -o|--ota)  PORT="$2"; MODE="ota"; shift 2 ;;
        -h|--help)
            echo "Uso: $0 [-p <porta>] [-o <ip>]"
            echo "  -p, --port   Porta serial (default: /dev/ttyUSB0)"
            echo "  -o, --ota    IP do gateway para upload OTA"
            exit 0 ;;
        *) echo "Opcao desconhecida: $1"; exit 1 ;;
    esac
done

if [[ "$MODE" == "ota" ]]; then
    echo "Building and uploading OTA to ${PORT}..."
    pio run -e esp8266_gateway_ota --target upload --upload-port "$PORT"
else
    echo "Building and flashing ESP8266 Gateway on ${PORT}..."
    pio run -e esp8266_gateway --target upload --upload-port "$PORT"
fi
echo "Done."
