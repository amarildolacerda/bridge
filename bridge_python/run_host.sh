#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

VENV_DIR="${VENV_DIR:-venv}"
HTTP_PORT="${HTTP_PORT:-8080}"
MQTT_HOST="${MQTT_HOST:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_USER="${MQTT_USER:-}"
MQTT_PASS="${MQTT_PASS:-}"
LOG_LEVEL="${LOG_LEVEL:-info}"

echo "=== bridge_python (host) ==="

if [ ! -d "$VENV_DIR" ]; then
    echo "[1/3] Criando venv..."
    python3 -m venv "$VENV_DIR"
fi

echo "[1/3] Verificando dependencias..."
"$VENV_DIR/bin/pip" install -q -r requirements.txt

echo "[2/3] Checando MQTT ($MQTT_HOST:$MQTT_PORT)..."
if command -v nc &>/dev/null; then
    if nc -z -w3 "$MQTT_HOST" "$MQTT_PORT" 2>/dev/null; then
        echo "  MQTT ok"
    else
        echo "  WARNING: MQTT broker nao respondendo em $MQTT_HOST:$MQTT_PORT"
    fi
else
    echo "  WARNING: nc nao disponivel, pulando verificacao MQTT"
fi

mkdir -p data

echo "[3/3] Iniciando bridge em http://0.0.0.0:$HTTP_PORT"
echo ""

export MQTT_HOST MQTT_PORT MQTT_USER MQTT_PASS HTTP_PORT LOG_LEVEL
exec "$VENV_DIR/bin/python" -m app.main
