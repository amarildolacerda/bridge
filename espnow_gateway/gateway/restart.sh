#!/bin/bash
set -e

if [[ $# -lt 1 ]]; then
    echo "Uso: $0 <ip>"
    echo "Envia POST /api/restart para o gateway no IP informado"
    exit 1
fi

IP="$1"
URL="http://${IP}/api/restart"

echo "Enviando POST ${URL}..."
RESP=$(curl -s -o /dev/null -w "%{http_code}" -X POST "$URL" -H "Content-Type: application/json" -d "{}")

case "$RESP" in
    200) echo "ok (HTTP 200)" ;;
    400) echo "erro: max sensors reached or already pairing" ;;
    *)   echo "falha: HTTP $RESP" ;;
esac
