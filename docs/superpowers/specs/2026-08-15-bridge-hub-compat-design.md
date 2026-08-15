# Bridge Python — Camada Hub-Compat (substitui o hub ESP32 para nodes TCP)

## Objetivo
Fazer o `bridge_python` atuar **como um hub** para nodes TCP que usam o `TcpNodeProtocol`
(`lamp` com `TCP_ENABLED`, `presence`, `climate-gas` e demais nodes que registram via HTTP+UDP).
Fluxo desejado: `node TCP → bridge → Home Assistant`.

## Restrições (confirmadas com o usuário)
- **Não** alterar o hub ESP32 nem o `lamp` (nem `shared/`).
- Implementação **somente** em `bridge/bridge_python` (Python).
- O bridge deve continuar servindo seus clients bridge-native (`/api/device/*`) sem regressão.
- `sensor_type` 9 (LIGHT, o `lamp`) → entidade **`light`** on/off no HA (decisão do usuário).

## Contexto técnico (extraído do código)
- `shared/src/tcp_protocol.h`: `TCP_UDP_PORT=5000`, `TCP_HTTP_PORT=80`.
  - `tcp_gw_discover_t` (packed, 34B): `{uint8_t msg_type=0x0A, uint8_t sensor_type, char device_name[32]}`
  - `tcp_gw_announce_t` (packed, 23B): `{uint8_t msg_type=0x09, uint8_t fw_version[4], char hub_ip[16], uint16_t hub_port}`
- `shared/src/common_types.h`: `payload_onoff_t { uint8_t state; }`
- `shared/src/sensor_type.h`: 1=TEMP_HUM, 2=CONTACT, 3=MOTION, 4=GAS, 5=RAIN, 6=LEVEL,
  7=DHT_GAS, 8=ONOFF, 9=LIGHT, 10=REPEATER, 11=DHT_RELE, 12=SOIL_MOISTURE.
- `lamp` (`shared/src/tcp_node_protocol.cpp`):
  - UDP discover binário → espera `tcp_gw_announce_t` (`hub_ip`,`hub_port`).
  - `POST /node/register` `{device_id, sensor_type, device_name, fw_version, mac, client_chip}` → espera `{status, assigned_slot, device_id}`.
  - `POST /node/state` `{device_id, ..., relay_state/state(bool), ip, free_heap, uptime_s, ...}`.
  - `POST /node/heartbeat` `{device_id}`.
  - `GET /node/command/<device_id>` → espera `{command:"on"|"off"|"restart"}`; `on_command(0x01/0x00)` ou `on_restart()`.
- Bridge atual (`bridge_python`):
  - UDP discovery é **JSON** (`{"service":"esp-bridge","discover":true}`) na porta 5000 — incompatível com o binário do node.
  - HTTP usa `/api/device/*` com campos `id`/`type` — incompatível.
  - **Não assina** tópicos de comando de device no MQTT (só `esp32-bridge/force_update/set`) → comando HA→device está ausente.
  - `mqtt_discovery.py`: `DEVICE_ENTITY_MAP` usa nomes de entidade (`power`, `temperature`, …); `publish_state` lê `dev.state.get(<entity_name>)`. Para ONOFF a entidade é `power`; não há entrada `light` on/off.

## Arquitetura
Módulo isolado `app/hub_compat.py` + extensões mínimas e localizadas em 4 arquivos existentes.
A face-TCP do hub é replicada no bridge; a fila de comandos hub-compat é **separada** da fila bridge-native.

### Componente 1 — UDP discovery binário (`app/udp_discovery.py`)
Estender `_handle_message(data, addr)`:
- Se `data` não vazio e `data[0] == 0x0A` (`MSG_GW_DISCOVER`): responder com `tcp_gw_announce_t` binário.
  - Packing: `struct.pack("<B4s16sH", 0x09, b"\x00\x00\x00\x00", ip16, 80)` onde `ip16 = bridge_ip.encode()[:15].ljust(16, b"\x00")`.
  - Enviar para `(addr[0], addr[1])` (mesmo socket/transport já existente).
- Caso contrário: manter o comportamento JSON atual (clients bridge-native).

### Componente 2 — Endpoints HTTP `/node/*` (`app/http_api.py`)
Adicionar rotas (não alterar as `/api/device/*`):
- `POST /node/register`: body `{device_id, sensor_type(int), device_name, fw_version, mac, client_chip}`.
  - `sensor_type` inválido (não no mapa) → 400.
  - Mapear → `DeviceType` (`hub_compat.SENSOR_TYPE_MAP`); `registry.register(device_id, dtype, device_name, ip="")`.
  - `mqtt.publish_device_config(dev)` + `ws_manager.notify_device_registered(dev)`.
  - Retornar `{"status":"ok","assigned_slot":<slot>,"device_id":device_id}`.
- `POST /node/state`: body `{device_id, ...campos de estado...}`.
  - Device inexistente → 404.
  - Para cada chave (exceto `device_id`): `dev.state[key]=value`.
  - Se `dtype` em (ONOFF, LIGHT): derivar bool de `state`/`relay_state` e gravar na chave de entidade HA
    (`"power"` para ONOFF, `"light"` para LIGHT).
  - `dev.last_seen=now; dev.online=True`; `ws_manager.notify_device_update(dev)`.
  - Retornar `{"status":"ok"}`.
- `POST /node/heartbeat`: `{device_id}` → marca `online`/`last_seen`; 404 se inexistente.
- `GET /node/command/{device_id}`: `cmd = registry.get_hub_command(device_id)` (pop front).
  - Se `cmd` None → `{}` (200). Senão → `{"command": cmd, "slot": <slot>}`.

### Componente 3 — Fila hub-compat (`app/device_registry.py`)
Adicionar `self._hub_commands: dict[str, list[str]]` e métodos:
- `enqueue_hub_command(device_id, cmd: str)` (append; cap `TCP_MAX_PENDING_COMMANDS`=10, descarta mais antigos).
- `get_hub_command(device_id) -> str | None` (pop front ou None).
Isolada de `commands` (bridge-native). Persistir junto no `save()`/`load()` (opcional; pode não persistir comandos).

### Componente 4 — Assinatura MQTT de comandos HA (`app/main.py`)
Novo task `hub_command_listener()` (espelha `force_update_listener`):
- `client.subscribe("homeassistant/+/+/+/set")`.
- Em mensagem: `parts = topic.split("/")` → `device_id = parts[2]`; `payload = msg.decode().strip().lower()`.
- Traduzir `"true"→"on"`, `"false"→"off"` (e aceitar `"on"`/`"off"` direto).
- Se `registry.get_device(device_id)` existir: `registry.enqueue_hub_command(device_id, cmd)`.
- Ignorar se device não registrado.

### Componente 5 — Mapa e tipos (`app/hub_compat.py` + `app/models.py`)
- `models.py`: adicionar `DeviceType.LIGHT = "light"`.
- `models.py` `DEVICE_ENTITY_MAP`: adicionar `DeviceType.LIGHT: [("light", "light", "", "", "")]`
  (entidade `light` on/off; `build_entity_config` já trata `platform=="light"` com payload_on/off).
- `hub_compat.py` `SENSOR_TYPE_MAP: dict[int, DeviceType]`:
  - 1→TEMPERATURE, 2→CONTACT, 3→OCCUPANCY, 4→GAS, 5→RAIN, 6→TANQUE, 7→DHT_GAS,
    8→ONOFF, 9→LIGHT, 11→ONOFF, 12→SOIL_MOISTURE. (10 REPEATER e demais → rejeitar com 400.)
- `hub_compat.py`: funções `handle_node_register/state/heartbeat/command_get` e `enqueue_hub_command`
  (orquestram registry + mqtt + ws), mantendo a lógica isolada.

## Fluxo de ponta a ponta
1. Node faz UDP discover (bin 0x0A) → bridge responde announce (bin 0x09, ip+porta 80).
2. Node `POST /node/register` → bridge registra + publica discovery HA (switch `power` ou `light` `light`).
3. Node `POST /node/state` (relay_state) → bridge grava `power`/`light` → `mqtt_state_sync` (5s) publica `…/state` → HA mostra estado.
4. HA toggle → `…/set`="true" → bridge assina → `enqueue_hub_command("on")` → node polling `GET /node/command/<id>` recebe `"on"` → `on_command(0x01)` → relay aciona → próximo `/node/state` reflete no HA.

## Tratamento de erro
- `sensor_type` fora do mapa → 400 em `/node/register`.
- Device inexistente em `/node/state` ou `/node/command` → 404 / `{}`.
- Comando HA para device não registrado → ignorado (log info).
- UDP discover com primeiro byte != 0x0A → segue caminho JSON atual.

## Testes (`bridge_python/tests/`)
- `test_hub_compat.py`:
  - Packing do `tcp_gw_announce_t` (tamanho 23, msg_type 0x09, hub_port=80 LE, ip null-padded).
  - `/node/register` → 200 + `assigned_slot`; `sensor_type` inválido → 400.
  - `/node/state` mapeia `relay_state`→`power` (ONOFF) e `state`→`light` (LIGHT).
  - `/node/command/<id>` retorna `"on"`/`"off"`/`"restart"` na ordem e `{}` quando vazio.
  - Tradução MQTT `"true"`→`"on"`, `"false"`→`"off"`.
  - `SENSOR_TYPE_MAP` cobre 1–9,11,12.
- Usar `fastapi.TestClient` + registry em memória (sem MQTT real).

## Fora de escopo
- ESP-NOW (bridge Python não tem rádio; nodes só ESP-NOW continuam precisando do hub físico).
- Comando `restart` via HA (fila suporta a string, mas não há gatilho HA obrigatório nesta fase).
- Alteração de clients bridge-native ou do hub.

## Riscos
- `bridge_ip` deve ser o IP real na LAN (config `bridge_ip` ou auto-detectado) para o node conectar.
- Latência de comando ~1s (poll do node) + 5s (sync MQTT) — aceitável para relay.
