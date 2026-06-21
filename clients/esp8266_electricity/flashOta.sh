#!/bin/bash
set -e
command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }

HOST=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--host) HOST="$2"; shift 2 ;;
        -help|--help)
            echo "Uso: $0 [-h <hostname>]"
            echo "  -h, --host   Hostname ou IP do dispositivo (default: <device_id>.local)"
            exit 0 ;;
        *) HOST="$1"; shift ;;
    esac
done

echo "Building..."
pio run

if [ -z "$HOST" ]; then
    echo "Discovering bridge IP..."
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    if ! HOST=$(python3 "$SCRIPT_DIR/../../discover_bridge.py" | grep -oP 'Bridge: \K[\d.]+' | head -1); then
        echo "Erro: especifique o host com -h ou certifique-se que o bridge esta acessivel via UDP"
        echo "Ex:  $0 -h esp8266_87c43e.local"
        exit 1
    fi
    if [ -z "$HOST" ]; then
        echo "Erro: nenhum bridge encontrado na rede"
        exit 1
    fi
    echo "Bridge encontrado em: $HOST"
fi

echo "Uploading OTA to $HOST ..."
pio run --target upload --upload-port "$HOST"
echo "OTA done!"
