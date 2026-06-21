# Python ESP32 Bridge Clone para Home Assistant

## Objetivo

Criar um clone Python do bridge ESP32 (existente em `main/`) que rode como **add-on do Home Assistant**, mantendo compatibilidade total com os clients ESP8266 existentes e expondo os dispositivos como entidades nativas do HA via **MQTT Discovery**.

O projeto fica em `bridge_python/` separado, sem alterar o código original do bridge ESP32.

## Stack

- **FastAPI** + uvicorn (HTTP + WebSocket)
- **aiomqtt** (cliente MQTT assíncrono)
- **asyncio** (UDP discovery, broadcast, timers)
- **Python 3.11+** (slim Docker image)

## Arquitetura

```
bridge_python/
├── Dockerfile                # HA add-on container
├── config.yaml               # HA add-on configuration
├── run.sh                    # Entrypoint do add-on
├── requirements.txt          # Dependências Python
└── app/
    ├── __init__.py
    ├── main.py               # FastAPI app, startup, event loop principal
    ├── config.py             # Config (env vars, HA add-on options)
    ├── device_registry.py    # Device storage + persistence JSON
    ├── mqtt_discovery.py     # MQTT Discovery publisher
    ├── udp_discovery.py      # UDP listener + broadcast
    ├── websocket_manager.py  # WebSocket connection manager
    ├── models.py             # Pydantic models (device types, state)
    └── web/
        ├── dashboard.html    # Dashboard (copiado do bridge original)
        └── dashboard.css     # Estilos (copiado do bridge original)
```

### Processos (mesmo event loop asyncio)

1. **HTTP Server** — FastAPI + uvicorn, porta 80
2. **UDP Listener** — socket asyncio, porta 5000, responde discovery + coleta broadcast de clients
3. **UDP Broadcast** — a cada 300s envia `re_register:true`
4. **MQTT Client** — conecta ao Mosquitto add-on, publica discovery e estado
5. **Heartbeat Monitor** — task periódica que marca devices offline se `last_seen > 60s`

## Device Registry

Estrutura de dados em memória (dict indexado por `id`) com persistência em JSON.

```python
@dataclass
class BridgedDevice:
    id: str                    # "esp8266_abcd12"
    name: str
    type: DeviceType           # enum string
    ip: str
    registered: bool = True
    online: bool = False
    last_seen: float = 0.0     # time.time()
    state: dict[str, float | bool | str] = field(default_factory=dict)
    commands: list[dict] = field(default_factory=list)
```

**Persistência:** `data/devices.json` — salvo a cada register/remove.

### Tipos de Device (11 tipos, mesmo enum do RainMaker)

| Tipo | String | Comportamento HA |
|---|---|---|
| `onoff` | switch | Power (bool) |
| `dimmable` | light | Power + Brightness |
| `temperature` | sensor | Temperature + Humidity |
| `humidity` | sensor | Humidity |
| `contact` | binary_sensor | Contact (invertido) |
| `occupancy` | binary_sensor | Occupancy |
| `light_sensor` | sensor | Light level |
| `tanque` | sensor | Level (%) |
| `gas` | binary_sensor + sensor | Gas alarm + Gas level |
| `rain` | binary_sensor + sensor | Rain digital + Rain level |
| `electricity` | sensor | Current (mA) |

### API do Registry

- `register(id, type, name, ip) -> slot`
- `remove(id) -> bool`
- `update_state(id, key, value) -> bool`
- `add_command(id, cluster, command, data) -> bool`
- `get_commands(id) -> list` (consome fila)
- `get_device(id) -> BridgedDevice | None`
- `get_all() -> list[BridgedDevice]`
- `get_by_index(idx) -> BridgedDevice | None`
- `check_heartbeats()` — marca offline se timeout
- `save() / load()` — persistência JSON

## MQTT Discovery

Para cada dispositivo registrado, publica auto-discovery topics no formato padrão HA.

### Tópicos

```
homeassistant/<platform>/<device_id>/<entity>/config  (JSON discovery)
homeassistant/<platform>/<device_id>/<entity>/state    (valor atual)
```

### Device Info (compartilhado entre entidades do mesmo device)

```json
{
  "identifiers": ["esp32_bridge_<device_id>"],
  "name": "<device_name>",
  "sw_version": "bridge_python_0.1.0",
  "manufacturer": "ESP RainMaker Gateway",
  "model": "<device_type>",
  "via_device": "esp32_bridge"
}
```

### Mapping Device → HA Entities

**temperature:**
- `sensor/<id>/temperature` — Temperature (°C)
- `sensor/<id>/humidity` — Humidity (%)

**gas:**
- `binary_sensor/<id>/alarm` — Gas alarm
- `sensor/<id>/gas_level` — Gas level (%)

**rain:**
- `binary_sensor/<id>/rain_digital` — Rain detected
- `sensor/<id>/rain_level` — Rain level (%)

**electricity:**
- `sensor/<id>/current` — Current (mA)

**contact:**
- `binary_sensor/<id>/contact` — Contact (device_class: door)

**occupancy:**
- `binary_sensor/<id>/occupancy` — Occupancy (device_class: occupancy)

**light_sensor:**
- `sensor/<id>/light` — Light level

**tanque:**
- `sensor/<id>/level` — Level (%)

**onoff:**
- `switch/<id>/power` — Power
- Command topic roteia para fila de comandos do device

**dimmable:**
- `light/<id>` — Light (Power + Brightness)
- Command topics roteiam para fila de comandos

### Comandos via MQTT

HA envia comandos para:
```
homeassistant/<platform>/<device_id>/<entity>/set
```

O bridge recebe, traduz para formato de comando interno e adiciona à fila do device:
- `onoff/set_onoff` com valor `true/false`
- `levelcontrol/set_level` com valor 0-255

## API HTTP

Mesmos endpoints do bridge ESP32 original (compatibilidade total com clients ESP8266).

| Método | Rota | Handler | Descrição |
|---|---|---|---|
| GET | `/` | dashboard_html | Dashboard web |
| GET | `/dashboard.css` | dashboard_css | CSS do dashboard |
| POST | `/api/device/register` | register_device | Registro de device |
| POST | `/api/device/remove` | remove_device | Remove device |
| POST | `/api/device/state` | device_state | Atualiza estado |
| GET/POST | `/api/device/commands` | device_commands | Poll de comandos |
| GET | `/api/device/info` | device_info | Info do device |
| GET | `/api/devices` | devices_list | Lista devices |
| GET | `/api/gateway/info` | gateway_info | Status do gateway |
| GET | `/api/ping` | ping | Health check |
| POST | `/api/device/heartbeat` | device_heartbeat | Heartbeat |
| POST | `/api/gateway/reset` | reset | Reinicia bridge |
| POST | `/api/gateway/broadcast` | broadcast | Broadcast re-register |
| GET | `/api/qrcode` | qrcode | QR code mock |
| POST | `/api/ota` | ota | OTA mock |
| GET | `/ws` | websocket | WebSocket monitor |

### Formatos de Resposta

```json
// POST /api/device/register
{"status":"ok","slot":0}

// POST /api/device/state
{"status":"ok"}

// GET /api/devices
[{"id":"esp8266_abcd","name":"Sensor","type":"temperature","ip":"192.168.1.100","online":true,"state":{"temperature":23.5,"humidity":65.0},"last_seen":1234567890}]

// GET /api/gateway/info
{"ip":"192.168.1.50","gateway":"192.168.1.1","netmask":"255.255.255.0","dns1":"8.8.8.8","dns2":"8.8.4.4","version":"v0.1.0","uptime_s":3600,"heap_free":123456,"total_devices":5}

// GET /api/ping
{"status":"ok"}

// GET /api/device/commands?id=xxx
{"commands":[{"cluster":"onoff","command":"set_onoff","data":"1"}]}

// GET /api/qrcode
{"service_name":"esp-bridge","pop":"","qrcode_url":"/api/qrcode"}
```

## UDP Discovery

Idêntico ao mecanismo do bridge ESP32.

### Listener (porta 5000)

- Escuta em `0.0.0.0:5000` UDP
- Responde a `{"service":"esp-bridge","discover":true}` com:
  ```json
  {"service":"esp-bridge","ip_sta":"<bridge_ip>","http_port":80}
  ```
- Se o request incluir `"id"`, armazena no cache de IPs descobertos
- Cache: `dict[id -> (ip, timestamp)]`, max 8 entries, timeout 300s

### Broadcast Periódico

- Task separada no event loop
- Delay inicial: 30s
- Intervalo: 300s
- Mensagem:
  ```json
  {"service":"esp-bridge","ip_sta":"<bridge_ip>","http_port":80,"uptime_s":<uptime>,"re_register":true}
  ```

### Broadcast Manual

`POST /api/gateway/broadcast` ou comando serial `b`:
- Envia broadcast UDP
- Aguarda 3s
- Retorna lista de devices registrados + descobertos

## WebSocket

Endpoint `/ws` — push de atualizações em tempo real para o dashboard.

Formato da mensagem:
```json
{"type":"device_update","device":{"id":"esp8266_xxx","state":{...}}}
{"type":"device_online","device":"esp8266_xxx"}
{"type":"device_offline","device":"esp8266_xxx"}
{"type":"device_registered","device":{...}}
{"type":"device_removed","device":"esp8266_xxx"}
```

## Web Dashboard

Mesmo HTML/CSS do bridge original, servido em `/` e `/dashboard.css`.

Funcionalidades mantidas:
- Hero card "ESP32 RainMaker Gateway"
- Admin dropdown: "Buscar dispositivos" (broadcast), "Reiniciar Gateway"
- Summary grid: IP, Uptime, Heap (memória livre do processo)
- Card "Status do Gateway"
- Card "Dispositivos" com LED online/offline
- WebSocket para real-time
- Polling fallback a cada 10s
- Toast notifications

## Home Assistant Add-on

### config.yaml

```yaml
name: "ESP32 Bridge Python"
version: "0.1.0"
slug: "esp32_bridge_python"
description: "ESP32 Bridge clone for Home Assistant with MQTT Discovery"
url: "https://github.com/anomalyco/bridge"
arch:
  - armhf
  - armv7
  - aarch64
  - amd64
  - i386
startup: "application"
boot: "auto"
init: false
ports:
  80/tcp: 8080
  5000/udp: 5000
ports_description:
  80/tcp: "HTTP API + Dashboard"
  5000/udp: "UDP Device Discovery"
options:
  mqtt_host: "core-mosquitto"
  mqtt_port: 1883
  mqtt_user: ""
  mqtt_pass: ""
  log_level: "info"
schema:
  mqtt_host: "str"
  mqtt_port: "int"
  mqtt_user: "str"
  mqtt_pass: "password"
  log_level: "list(debug|info|warn|error)"
```

### Dockerfile

```dockerfile
FROM python:3.11-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
RUN mkdir -p data
EXPOSE 80/tcp
EXPOSE 5000/udp
CMD ["python", "-m", "app.main"]
```

### run.sh

```bash
#!/usr/bin/ash
python -m app.main
```

## Configuração

Variáveis de ambiente (HA add-on options):

| Variável | Default | Descrição |
|---|---|---|
| `MQTT_HOST` | `core-mosquitto` | Host do broker MQTT |
| `MQTT_PORT` | `1883` | Porta do broker MQTT |
| `MQTT_USER` | `""` | Usuário MQTT |
| `MQTT_PASS` | `""` | Senha MQTT |
| `LOG_LEVEL` | `info` | Nível de log |
| `HTTP_PORT` | `80` | Porta HTTP |
| `DISCOVERY_PORT` | `5000` | Porta UDP discovery |

## Dependências (requirements.txt)

```
fastapi>=0.110.0
uvicorn[standard]>=0.27.0
aiomqtt>=2.0.0
pydantic>=2.0.0
aiofiles>=23.0
```

## Estrutura de Implementação

### Fase 1 — Core (device_registry + HTTP API)
1. Models Pydantic
2. Device registry com persistência JSON
3. Servidor FastAPI com todos endpoints HTTP
4. Dashboard web embarcado

### Fase 2 — UDP Discovery
1. UDP listener com resposta a discovery
2. Broadcast periódico
3. Cache de IPs descobertos

### Fase 3 — MQTT Discovery
1. Conexão MQTT
2. Publicação de discovery config
3. State updates via MQTT
4. Command handling via MQTT

### Fase 4 — HA Add-on
1. Dockerfile
2. config.yaml
3. run.sh
4. Teste de integração

### Fase 5 — WebSocket + Final
1. WebSocket manager
2. Integração com dashboard
3. Heartbeat monitor
4. Testes
