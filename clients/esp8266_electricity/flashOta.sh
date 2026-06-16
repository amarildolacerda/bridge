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
    echo "Hostname nao especificado."
    echo "Use: $0 -h <hostname>"
    echo "Ex:  $0 -h esp8266_87c43e.local"
    exit 1
fi

echo "Uploading OTA to $HOST ..."
pio run --target upload --upload-port "$HOST"
echo "OTA done!"
