#!/bin/bash
set -e

MQTT_USER="${1:-bridge}"
MQTT_PASS="${2:-bridge123}"
BRIDGE_DIR="/home/kzuca/project/bridge/bridge_python"

echo "=== Criando config Mosquitto ==="
mkdir -p /tmp/mosquitto/config /tmp/mosquitto/data

cat > /tmp/mosquitto/config/mosquitto.conf << 'EOF'
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
persistence true
persistence_location /var/lib/mosquitto/
EOF

echo "=== Criando usuario $MQTT_USER ==="
docker run --rm -v /tmp/mosquitto/config:/etc/mosquitto \
  eclipse-mosquitto:2 \
  mosquitto_passwd -c -b /etc/mosquitto/passwd "$MQTT_USER" "$MQTT_PASS"

echo "=== Subindo Mosquitto ==="
docker rm -f mosquitto 2>/dev/null || true
docker run -d --name mosquitto --restart unless-stopped \
  --network host \
  -v /tmp/mosquitto/config:/etc/mosquitto \
  -v /tmp/mosquitto/data:/var/lib/mosquitto \
  eclipse-mosquitto:2

echo "=== Configurando MQTT no HA ==="
HA_CONFIG_DIR=$(docker inspect homeassistant --format '{{range .Mounts}}{{if eq .Destination "/config"}}{{.Source}}{{end}}{{end}}')
echo "HA config dir: $HA_CONFIG_DIR"

if [ -n "$HA_CONFIG_DIR" ] && [ -f "$HA_CONFIG_DIR/configuration.yaml" ]; then
  if ! grep -q "mqtt:" "$HA_CONFIG_DIR/configuration.yaml"; then
    cat >> "$HA_CONFIG_DIR/configuration.yaml" << EOF

# MQTT
mqtt:
  broker: localhost
  port: 1883
  username: $MQTT_USER
  password: $MQTT_PASS
EOF
    docker restart homeassistant
    echo "HA reiniciado com config MQTT"
  else
    echo "MQTT ja configurado no HA"
  fi
fi

echo "=== Build bridge ==="
docker build -t esp32-bridge-python "$BRIDGE_DIR"

echo "=== Rodando bridge ==="
docker rm -f esp32-bridge 2>/dev/null || true
docker run -d --name esp32-bridge --restart unless-stopped \
  --network host \
  -e MQTT_HOST=localhost \
  -e MQTT_PORT=1883 \
  -e MQTT_USER=$MQTT_USER \
  -e MQTT_PASS=$MQTT_PASS \
  -v bridge_data:/app/data \
  esp32-bridge-python

echo ""
echo "=== PRONTO ==="
echo "Mosquitto: localhost:1883 (user: $MQTT_USER / $MQTT_PASS)"
echo "Bridge:    http://localhost:8080"
echo "Teste:     curl http://localhost:8080/api/ping"
EOF
