#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ADDON_NAME="esp32-bridge-python"
CANDIDATES=(
    /addons
    /usr/share/hassio/addons
    /mnt/data/supervisor/addons
    "$HOME/homeassistant/addons"
    /var/lib/docker/volumes/hassio_addons/_data
)

MQTT_HOST="${MQTT_HOST:-core-mosquitto}"
MQTT_PORT="${MQTT_PORT:-1883}"

_clean() {
    echo "== Limpeza MQTT: registros antigos do bridge ESP32 =="
    echo ""
    "$DIR/cleanup_mqtt_bridge.py" --host "$MQTT_HOST" --port "$MQTT_PORT"
}

_install() {
    local target="$1"
    local addon_dir="$target/$ADDON_NAME"

    echo "== Instalador Add-on HA: ESP32 Bridge Python =="
    echo ""
    echo "Destino: $addon_dir"

    if [ ! -d "$target" ]; then
        echo "ERRO: diretorio '$target' nao existe."
        exit 1
    fi

    mkdir -p "$addon_dir"
    rsync -av --delete \
        --exclude='venv/' \
        --exclude='__pycache__/' \
        --exclude='.pytest_cache/' \
        --exclude='.git/' \
        --exclude='tests/' \
        --exclude='*.pyc' \
        --exclude='install_addon.sh' \
        --exclude='cleanup_mqtt_bridge.py' \
        --exclude='run_host.sh' \
        --exclude='README.md' \
        --exclude='HA_INTEGRATION.md' \
        "$DIR/" "$addon_dir/"

    echo ""
    echo "Add-on instalado em: $addon_dir"
    echo ""

    read -r -p "Limpar registros antigos do bridge ESP32 no MQTT ($MQTT_HOST:$MQTT_PORT)? [s/N] " resp
    if [[ "$resp" =~ ^[Ss]$ ]]; then
        _clean
    fi

    echo ""
    echo "Proximos passos no Home Assistant:"
    echo "  1. Settings > Add-ons > Supervisor > Add-on Store"
    echo "  2. Menu (3 pontinhos) > Reload"
    echo "  3. O add-on 'ESP32 Bridge Python' aparecera na lista"
    echo "  4. Instalar e iniciar"
}

_cmd="${1:-help}"

case "$_cmd" in
    install)
        if [ -z "$2" ]; then
            echo "Uso: $0 install <diretorio_de_addons>"
            echo "Ex:  $0 install /addons"
            exit 1
        fi
        _install "$2"
        ;;
    clean)
        _clean
        ;;
    help|--help|-h|"")
        echo "Uso: $0 <comando> [argumentos]"
        echo ""
        echo "Comandos:"
        echo "  install <dir>   Instala o add-on no diretorio de addons do HA"
        echo "  clean           Remove registros antigos do bridge ESP32 no MQTT"
        echo "  help            Mostra esta mensagem"
        echo ""
        echo "Diretorios de addons encontrados neste sistema:"
        FOUND=0
        for d in "${CANDIDATES[@]}"; do
            if [ -d "$d" ]; then
                echo "  - $d"
                FOUND=1
            fi
        done
        if [ "$FOUND" -eq 0 ]; then
            echo "  (nenhum encontrado)"
        fi
        echo ""
        echo "Exemplos:"
        echo "  $0 install /addons"
        echo "  $0 install /usr/share/hassio/addons"
        echo "  $0 clean"
        ;;
    *)
        echo "Comando desconhecido: $_cmd"
        echo "Uso: $0 install <dir> | clean | help"
        exit 1
        ;;
esac
