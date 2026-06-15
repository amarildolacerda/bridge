#!/bin/bash
set -e
cd "$(dirname "$0")"
source config.sh
echo "==> Monitor padrão (Ctrl+] para sair)"
idf.py monitor -p /dev/ttyUSB0
