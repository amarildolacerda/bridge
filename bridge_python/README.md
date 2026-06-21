# ESP32 Bridge Python — Home Assistant Add-on

Bridge em Python compatível com os clients ESP8266, roda como add-on do Home Assistant ou container Docker. Substitui o bridge ESP32 físico expondo devices como entidades nativas do HA via MQTT Discovery.

## Funcionalidades

- API REST idêntica ao bridge ESP32 (registro, state, heartbeat, comandos)
- Descoberta UDP (porta 5000, service `esp-bridge`)
- MQTT Discovery para Home Assistant (`homeassistant/` prefix)
- WebSocket para atualizações em tempo real
- Dashboard web embarcado
- Persistência de devices em JSON
- Compatível com todos os clients ESP8266 existentes

## Device Types Suportados

| Tipo | Platforma HA | Descrição |
|------|-------------|-----------|
| `onoff` | `switch` | Relé liga/desliga |
| `dimmable` | `light` | Brilho ajustável |
| `temperature` | `sensor` | Temperatura |
| `humidity` | `sensor` | Umidade |
| `contact` | `binary_sensor` | Sensor magnético |
| `occupancy` | `binary_sensor` | Sensor de presença |
| `light_sensor` | `sensor` | Luminosidade |
| `tanque` | `sensor` | Nível de água/tanque |
| `gas` | `sensor` | Gás |
| `rain` | `sensor` | Chuva |
| `electricity` | `sensor` | Energia elétrica |

## Configuração

Via variáveis de ambiente:

| Variável | Padrão | Descrição |
|----------|--------|-----------|
| `MQTT_HOST` | `core-mosquitto` | Host do broker MQTT |
| `MQTT_PORT` | `1883` | Porta do broker |
| `MQTT_USER` | `"bridge"` | Usuário MQTT |
| `MQTT_PASS` | `"bridge123"` | Senha MQTT |
| `HTTP_PORT` | `80` | Porta do HTTP API |
| `LOG_LEVEL` | `info` | Nível de log (`debug`, `info`, `warn`, `error`) |

## Desenvolvimento

```bash
cd bridge_python
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python -m app.main
```

### Testes

```bash
pytest -v
```

### Docker

```bash
docker build -t esp32-bridge-python .
docker run -d --name esp32-bridge --network host \
  -e MQTT_HOST=localhost \
  -e MQTT_PORT=1883 \
  esp32-bridge-python
```

### Home Assistant + Mosquitto

```bash
./setup_mqtt.sh          # default: bridge / bridge123
```

## API

| Método | Rota | Descrição |
|--------|------|-----------|
| GET | `/api/ping` | Health check |
| POST | `/api/device/register` | Registrar device |
| POST | `/api/device/remove` | Remover device |
| POST | `/api/device/state` | Atualizar estado |
| GET | `/api/device/commands` | Comandos pendentes |
| POST | `/api/device/heartbeat` | Heartbeat |
| GET | `/api/devices` | Listar devices |
| GET | `/api/gateway/info` | Status do gateway |
| POST | `/api/gateway/broadcast` | Broadcast re-register |
| WS | `/ws` | WebSocket |
| GET | `/` | Dashboard web |

## Estrutura

```
bridge_python/
├── app/
│   ├── main.py              # Entrypoint FastAPI
│   ├── config.py            # Config via env vars
│   ├── models.py            # Dataclasses e enums
│   ├── device_registry.py   # Registro + persistência
│   ├── http_api.py          # Rotas REST
│   ├── udp_discovery.py     # Descoberta UDP
│   ├── mqtt_discovery.py    # MQTT Discovery HA
│   ├── websocket_manager.py # WebSocket broadcast
│   └── web/                 # Dashboard HTML+CSS
├── tests/                   # Testes pytest
├── Dockerfile               # Container HA
├── config.yaml              # Add-on config
└── run.sh                   # Entrypoint
```
