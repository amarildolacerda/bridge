#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

HOST=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--host) HOST="$2"; shift 2 ;;
        -help|--help)
            echo "Uso: $0 -h <hostname|ip>"
            echo "  -h, --host   IP ou hostname do bridge"
            exit 0 ;;
        *) HOST="$1"; shift ;;
    esac
done

if [ -z "$HOST" ]; then
    echo "Erro: especifique o host com -h"
    echo "Ex:  $0 -h 192.168.1.105"
    exit 1
fi

cd "$SCRIPT_DIR"

if ! command -v idf.py &>/dev/null; then
    echo "ESP-IDF not loaded. Sourcing config.sh..."
    source "$SCRIPT_DIR/config.sh"
fi

FIRMWARE="$SCRIPT_DIR/build/esp_rainmaker_gateway.bin"

if [ ! -f "$FIRMWARE" ]; then
    echo "Firmware nao encontrado. Buildando..."
    idf.py build
fi

echo "Uploading OTA to $HOST ..."
curl -s -X POST "http://$HOST/api/ota" \
    --data-binary "@$FIRMWARE" \
    -H "Content-Type: application/octet-stream" \
    -o /dev/null -w "HTTP %{http_code}\n"

echo "OTA done! O dispositivo sera reiniciado."
