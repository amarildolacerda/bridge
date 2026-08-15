# Bridge Python — Camada Hub-Compat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fazer o `bridge_python` atuar como hub para nodes TCP (`lamp` `TCP_ENABLED` + outros), replicando a face-TCP do hub ESP32 (UDP discover binário + endpoints `/node/*` + fila de comandos + comando HA→node), fluxo `node → bridge → HA`.

**Architecture:** Módulo isolado `app/hub_compat.py` com o mapa `sensor_type`→`DeviceType` e os handlers assíncronos; fila de comandos hub-compat isolada na `DeviceRegistry`; extensão mínima no `udp_discovery.py` (detecta `0x0A` e responde announce binário), `http_api.py` (rotas `/node/*`), `main.py` (listener MQTT `homeassistant/+/+/+/set`). Clients bridge-native (`/api/device/*`) permanecem intactos.

**Tech Stack:** Python 3.11+, FastAPI, aiomqtt, pytest + pytest-asyncio (`asyncio_mode=auto`), `fastapi.testclient.TestClient`.

## Global Constraints

- **Não** alterar o hub ESP32 nem o `lamp` (nem `shared/`); implementação **somente** em `bridge/bridge_python`.
- `sensor_type` 9 (LIGHT, o `lamp`) → entidade **`light`** on/off no HA (decisão do usuário); 8 (ONOFF) → `switch` (`power`).
- Manter clients bridge-native (`/api/device/*`) funcionando sem regressão.
- `bridge_ip` deve ser o IP real na LAN (config `bridge_ip` ou auto-detectado) para o node conectar.
- Trabalhar no branch **`dev`** do bridge (nunca `main`); **não fazer push** sem confirmação do usuário.
- Estruturas binárias exatas (de `shared/src/tcp_protocol.h`): `tcp_gw_discover_t` (34B, `msg_type=0x0A`), `tcp_gw_announce_t` (23B, `msg_type=0x09`, `hub_port` little-endian). `TCP_UDP_PORT=5000`, `TCP_HTTP_PORT=80`.

---

## File Structure

- `app/models.py` — adicionar `DeviceType.LIGHT = "light"`.
- `app/mqtt_discovery.py` — adicionar entrada `DeviceType.LIGHT` em `DEVICE_ENTITY_MAP`.
- `app/device_registry.py` — adicionar `self._hub_commands`, métodos `enqueue_hub_command`, `get_hub_command`, `slot_of`.
- `app/hub_compat.py` (NOVO) — `SENSOR_TYPE_MAP`, `RELAY_STATE_KEY`, handlers assíncronos `handle_node_register/state/heartbeat/command_get`, helpers `translate_ha_payload`, `device_id_from_command_topic`, `build_announce_bytes`.
- `app/udp_discovery.py` — detectar `0x0A` em `_handle_message` e responder announce binário.
- `app/http_api.py` — rotas `POST /node/register`, `POST /node/state`, `POST /node/heartbeat`, `GET /node/command/{device_id}`.
- `app/main.py` — `hub_command_listener()` assinando `homeassistant/+/+/+/set`; criar task no `startup()`.
- `tests/test_hub_compat.py` — Tasks 1,2,3,6.
- `tests/test_udp_discovery.py` — Task 4.
- `tests/test_http_node.py` — Task 5.
- `tests/test_hub_integration.py` — Task 7.

---

### Task 1: Tipo de device LIGHT + entidade MQTT

**Files:**
- Modify: `bridge/bridge_python/app/models.py` (enum `DeviceType`)
- Modify: `bridge/bridge_python/app/mqtt_discovery.py` (`DEVICE_ENTITY_MAP`)
- Test: `bridge/bridge_python/tests/test_hub_compat.py`

**Interfaces:**
- Produces: `DeviceType.LIGHT` (valor `"light"`); `DEVICE_ENTITY_MAP[DeviceType.LIGHT] == [("light","light","","","")]`.

- [ ] **Step 1: Write the failing test**

```python
# tests/test_hub_compat.py
from app.models import DeviceType
from app.mqtt_discovery import DEVICE_ENTITY_MAP


def test_light_device_type_exists():
    assert DeviceType.LIGHT.value == "light"


def test_light_entity_map_entry():
    assert DEVICE_ENTITY_MAP[DeviceType.LIGHT] == [("light", "light", "", "", "")]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py -v`
Expected: FAIL (`AttributeError: LIGHT` / `KeyError`).

- [ ] **Step 3: Write minimal implementation**

In `app/models.py`, add `LIGHT = "light"` to the `DeviceType` enum (após `LIGHT_SENSOR`):

```python
    LIGHT_SENSOR = "light_sensor"
    TANQUE = "tanque"
    GAS = "gas"
    DHT_GAS = "dht_gas"
    RAIN = "rain"
    SOIL_MOISTURE = "soil_moisture"
    ELECTRICITY = "electricity"
    LIGHT = "light"
    BRIDGE = "bridge"
    UNKNOWN = "unknown"
```

In `app/mqtt_discovery.py`, add to `DEVICE_ENTITY_MAP` (após a entrada `ONOFF`):

```python
    DeviceType.ONOFF: [("power", "switch", "", "", "")],
    DeviceType.LIGHT: [("light", "light", "", "", "")],
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/models.py app/mqtt_discovery.py tests/test_hub_compat.py
git commit -m "feat(bridge): add LIGHT device type and light entity map entry"
```

---

### Task 2: Fila de comandos hub-compat na DeviceRegistry

**Files:**
- Modify: `bridge/bridge_python/app/device_registry.py`
- Test: `bridge/bridge_python/tests/test_hub_compat.py`

**Interfaces:**
- Consumes: `DeviceRegistry` existente (`register`, `get_device`, `get_all`).
- Produces: `enqueue_hub_command(device_id, cmd) -> bool`, `get_hub_command(device_id) -> str|None`, `slot_of(device_id) -> int`.

- [ ] **Step 1: Write the failing test**

```python
# append to tests/test_hub_compat.py
from app.device_registry import DeviceRegistry
from app.models import DeviceType


def test_hub_command_queue(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    reg.register("agri_123", DeviceType.ONOFF, "Lamp", "")
    assert reg.enqueue_hub_command("agri_123", "on") is True
    assert reg.enqueue_hub_command("nope", "on") is False
    assert reg.get_hub_command("agri_123") == "on"
    assert reg.get_hub_command("agri_123") is None
    assert reg.slot_of("agri_123") == 0
    assert reg.slot_of("missing") == -1


def test_hub_command_queue_cap(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    reg.register("agri_cap", DeviceType.ONOFF, "Cap", "")
    for i in range(15):
        reg.enqueue_hub_command("agri_cap", f"c{i}")
    # cap em 10: os 5 primeiros foram descartados
    assert reg.get_hub_command("agri_cap") == "c5"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py::test_hub_command_queue -v`
Expected: FAIL (`AttributeError: enqueue_hub_command`).

- [ ] **Step 3: Write minimal implementation**

In `app/device_registry.py`, em `__init__` adicionar o dict:

```python
        self._devices: dict[str, BridgedDevice] = {}
        self._hub_commands: dict[str, list[str]] = {}
        self._data_dir = data_dir
```

Adicionar os métodos (fora de `__init__`, junto dos demais):

```python
    def enqueue_hub_command(self, device_id: str, cmd: str) -> bool:
        if device_id not in self._devices:
            return False
        q = self._hub_commands.setdefault(device_id, [])
        q.append(cmd)
        while len(q) > 10:
            q.pop(0)
        return True

    def get_hub_command(self, device_id: str) -> str | None:
        q = self._hub_commands.get(device_id)
        if not q:
            return None
        return q.pop(0)

    def slot_of(self, device_id: str) -> int:
        for i, d in enumerate(self.get_all()):
            if d.id == device_id:
                return i
        return -1
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/device_registry.py tests/test_hub_compat.py
git commit -m "feat(bridge): add isolated hub-compat command queue to registry"
```

---

### Task 3: Handlers hub-compat (`app/hub_compat.py`)

**Files:**
- Create: `bridge/bridge_python/app/hub_compat.py`
- Test: `bridge/bridge_python/tests/test_hub_compat.py`

**Interfaces:**
- Consumes: `DeviceRegistry` (Task 2), `DeviceType`, `mqtt.publish_device_config(dev)` (async), `ws_manager.notify_device_registered/update(dev)` (async).
- Produces: `handle_node_register(registry, mqtt, ws_manager, body) -> (dict, int)`, `handle_node_state(registry, ws_manager, body) -> (dict, int)`, `handle_node_heartbeat(registry, body) -> (dict, int)`, `handle_node_command_get(registry, device_id) -> (dict, int)`, `translate_ha_payload(payload) -> str|None`, `device_id_from_command_topic(topic) -> str|None`, `build_announce_bytes(ip, port) -> bytes`.

- [ ] **Step 1: Write the failing test**

```python
# append to tests/test_hub_compat.py
from unittest.mock import AsyncMock
from app import hub_compat


@pytest.mark.asyncio
async def test_register_state_command_cycle(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    mqtt = AsyncMock()
    ws = AsyncMock()
    resp, code = await hub_compat.handle_node_register(
        reg, mqtt, ws,
        {"device_id": "agri_abc", "sensor_type": 9, "device_name": "Lamp"},
    )
    assert code == 200
    assert resp["assigned_slot"] == 0
    assert resp["device_id"] == "agri_abc"
    mqtt.publish_device_config.assert_awaited_once()

    resp, code = await hub_compat.handle_node_state(
        reg, ws,
        {"device_id": "agri_abc", "relay_state": True, "ip": "1.2.3.4"},
    )
    assert code == 200
    dev = reg.get_device("agri_abc")
    assert dev.state["light"] is True
    assert dev.ip == "1.2.3.4"

    reg.enqueue_hub_command("agri_abc", "on")
    resp, code = await hub_compat.handle_node_command_get(reg, "agri_abc")
    assert code == 200
    assert resp == {"command": "on", "slot": 0}

    resp, code = await hub_compat.handle_node_command_get(reg, "agri_abc")
    assert resp == {}


@pytest.mark.asyncio
async def test_invalid_sensor_type(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    resp, code = await hub_compat.handle_node_register(
        reg, None, None, {"device_id": "x", "sensor_type": 99}
    )
    assert code == 400


def test_translate_and_topic():
    assert hub_compat.translate_ha_payload("true") == "on"
    assert hub_compat.translate_ha_payload("FALSE") == "off"
    assert hub_compat.translate_ha_payload("on") == "on"
    assert hub_compat.translate_ha_payload("bogus") is None
    assert hub_compat.device_id_from_command_topic("homeassistant/light/agri_x/light/set") == "agri_x"
    assert hub_compat.device_id_from_command_topic("homeassistant/switch/agri_x/power/set") == "agri_x"
    assert hub_compat.device_id_from_command_topic("esp32-bridge/force_update/set") is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py::test_register_state_command_cycle -v`
Expected: FAIL (`ModuleNotFoundError: app.hub_compat`).

- [ ] **Step 3: Write minimal implementation**

Create `app/hub_compat.py`:

```python
from __future__ import annotations
import struct
import time
from typing import Any

from app.models import DeviceType

SENSOR_TYPE_MAP: dict[int, DeviceType] = {
    1: DeviceType.TEMPERATURE,
    2: DeviceType.CONTACT,
    3: DeviceType.OCCUPANCY,
    4: DeviceType.GAS,
    5: DeviceType.RAIN,
    6: DeviceType.TANQUE,
    7: DeviceType.DHT_GAS,
    8: DeviceType.ONOFF,
    9: DeviceType.LIGHT,
    11: DeviceType.ONOFF,
    12: DeviceType.SOIL_MOISTURE,
}

# Chave de estado HA para tipos relay-like
RELAY_STATE_KEY: dict[DeviceType, str] = {
    DeviceType.ONOFF: "power",
    DeviceType.LIGHT: "light",
}


def translate_ha_payload(payload: str | None) -> str | None:
    p = (payload or "").strip().lower()
    if p in ("true", "on"):
        return "on"
    if p in ("false", "off"):
        return "off"
    return None


def device_id_from_command_topic(topic: str) -> str | None:
    parts = topic.split("/")
    if len(parts) == 5 and parts[0] == "homeassistant" and parts[4] == "set":
        return parts[2]
    return None


def build_announce_bytes(bridge_ip: str, http_port: int) -> bytes:
    ip_bytes = bridge_ip.encode("utf-8")[:15].ljust(16, b"\x00")
    return struct.pack("<B4s16sH", 0x09, b"\x00\x00\x00\x00", ip_bytes, http_port)


async def handle_node_register(registry, mqtt, ws_manager, body: dict) -> tuple[dict, int]:
    device_id = body.get("device_id")
    sensor_type = body.get("sensor_type")
    device_name = body.get("device_name", device_id)
    if not device_id_or_name(device_id) or sensor_type is None:
        return {"status": "error", "message": "missing fields"}, 400
    try:
        stype = int(sensor_type)
    except (TypeError, ValueError):
        return {"status": "error", "message": "invalid sensor_type"}, 400
    dtype = SENSOR_TYPE_MAP.get(stype)
    if dtype is None:
        return {"status": "error", "message": "invalid sensor_type"}, 400
    slot = registry.register(device_id, dtype, device_name, "")
    dev = registry.get_device(device_id)
    if mqtt is not None and dev is not None:
        await mqtt.publish_device_config(dev)
    if ws_manager is not None and dev is not None:
        await ws_manager.notify_device_registered(dev)
    return {"status": "ok", "assigned_slot": slot, "device_id": device_id}, 200


async def handle_node_state(registry, ws_manager, body: dict) -> tuple[dict, int]:
    device_id = body.get("device_id")
    if not device_id:
        return {"status": "error", "message": "missing device_id"}, 400
    dev = registry.get_device(device_id)
    if dev is None:
        return {"status": "error", "message": "device not found"}, 404
    for key, value in body.items():
        if key == "device_id":
            continue
        dev.state[key] = value
    if isinstance(body.get("ip"), str):
        dev.ip = body["ip"]
    if dev.type in RELAY_STATE_KEY:
        val = None
        if "state" in body:
            val = bool(body["state"])
        elif "relay_state" in body:
            val = bool(body["relay_state"])
        if val is not None:
            dev.state[RELAY_STATE_KEY[dev.type]] = val
    dev.last_seen = time.time()
    dev.online = True
    if ws_manager is not None:
        await ws_manager.notify_device_update(dev)
    return {"status": "ok"}, 200


async def handle_node_heartbeat(registry, body: dict) -> tuple[dict, int]:
    device_id = body.get("device_id")
    if not device_id:
        return {"status": "error", "message": "missing device_id"}, 400
    dev = registry.get_device(device_id)
    if dev is None:
        return {"status": "error", "message": "device not found"}, 404
    dev.last_seen = time.time()
    dev.online = True
    return {"status": "ok"}, 200


async def handle_node_command_get(registry, device_id: str) -> tuple[dict, int]:
    if not device_id:
        return {"status": "error", "message": "missing device_id"}, 400
    dev = registry.get_device(device_id)
    if dev is None:
        return {"status": "error", "message": "device not found"}, 404
    cmd = registry.get_hub_command(device_id)
    if cmd is None:
        return {}, 200
    return {"command": cmd, "slot": registry.slot_of(device_id)}, 200


def device_id_or_name(v) -> bool:
    return bool(v)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/hub_compat.py tests/test_hub_compat.py
git commit -m "feat(bridge): add hub-compat handlers and HA payload translation"
```

---

### Task 4: UDP discover binário no `udp_discovery.py`

**Files:**
- Modify: `bridge/bridge_python/app/udp_discovery.py` (`_handle_message`, adicionar `_send_binary_announce`)
- Test: `bridge/bridge_python/tests/test_udp_discovery.py`

**Interfaces:**
- Consumes: `build_announce_bytes` (Task 3), `self._bridge_ip`, `self._http_port`, `self._sock`.
- Produces: resposta binária `tcp_gw_announce_t` (23B, `0x09`) ao receber `0x0A`.

- [ ] **Step 1: Write the failing test**

```python
# tests/test_udp_discovery.py
import struct
from app.udp_discovery import build_announce_bytes, UDPDiscovery


def test_build_announce_bytes():
    b = build_announce_bytes("192.168.1.50", 80)
    assert len(b) == 23
    msg_type, fw, ip, port = struct.unpack("<B4s16sH", b)
    assert msg_type == 0x09
    assert port == 80
    assert ip.rstrip(b"\x00").decode() == "192.168.1.50"


def test_handle_binary_discover_sends_announce():
    udp = UDPDiscovery(bridge_ip="192.168.1.50", http_port=80)
    sent = []

    class FakeSock:
        def sendto(self, data, addr):
            sent.append((data, addr))
            return len(data)

    udp._sock = FakeSock()
    # tcp_gw_discover_t: msg_type=0x0A, sensor_type=9, device_name[32]
    discover = bytes([0x0A, 9]) + b"\x00" * 32
    udp._handle_message(discover, ("10.0.0.5", 1234))
    assert len(sent) == 1
    data, addr = sent[0]
    assert addr == ("10.0.0.5", 1234)
    assert data[0] == 0x09
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd bridge/bridge_python && pytest tests/test_udp_discovery.py -v`
Expected: FAIL (`ImportError: cannot import name 'build_announce_bytes'`).

- [ ] **Step 3: Write minimal implementation**

In `app/udp_discovery.py`, import `build_announce_bytes` no topo (após imports) e modificar `_handle_message`:

```python
from app.hub_compat import build_announce_bytes
```

```python
    def _handle_message(self, data: bytes, addr: tuple[str, int]):
        # Hub-compat: binary MSG_GW_DISCOVER (0x0A) from TCP nodes
        if data and data[0] == 0x0A:
            self._send_binary_announce(addr[0], addr[1])
            return
        try:
            msg = json.loads(data.decode())
        except (json.JSONDecodeError, UnicodeDecodeError):
            return
        service = msg.get("service")
        if service != DISCOVERY_SERVICE:
            return
        if msg.get("discover") is True:
            sender_id = msg.get("id", addr[0])
            self._discovered_ips[sender_id] = (addr[0], time.time())
            self._prune_discovered()
            self._send_response(addr[0], addr[1])
```

Adicionar método `_send_binary_announce`:

```python
    def _send_binary_announce(self, target_ip: str, target_port: int):
        payload = build_announce_bytes(self._bridge_ip, self._http_port)
        try:
            if self._sock:
                self._sock.sendto(payload, (target_ip, target_port))
        except Exception:
            LOG.exception("UDP binary announce send error")
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd bridge/bridge_python && pytest tests/test_udp_discovery.py -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/udp_discovery.py tests/test_udp_discovery.py
git commit -m "feat(bridge): respond to binary TCP-node UDP discover with announce"
```

---

### Task 5: Rotas HTTP `/node/*` no `http_api.py`

**Files:**
- Modify: `bridge/bridge_python/app/http_api.py`
- Test: `bridge/bridge_python/tests/test_http_node.py`

**Interfaces:**
- Consumes: `handle_node_register/state/heartbeat/command_get` (Task 3), `registry` (closure global), `request.app.state.ws_manager`, `request.app.state.mqtt`.
- Produces: endpoints `POST /node/register`, `POST /node/state`, `POST /node/heartbeat`, `GET /node/command/{device_id}`.

- [ ] **Step 1: Write the failing test**

```python
# tests/test_http_node.py
from fastapi.testclient import TestClient
from app.device_registry import DeviceRegistry
from app.websocket_manager import WebSocketManager
from app.http_api import create_app


def _client(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    ws = WebSocketManager()
    app = create_app(reg, ws)
    app.state.mqtt = None
    return TestClient(app), reg


def test_node_endpoints(tmp_path):
    client, reg = _client(tmp_path)
    r = client.post("/node/register", json={"device_id": "agri_x", "sensor_type": 9, "device_name": "Lamp"})
    assert r.status_code == 200
    assert r.json()["assigned_slot"] == 0

    r = client.post("/node/state", json={"device_id": "agri_x", "relay_state": True})
    assert r.status_code == 200
    assert reg.get_device("agri_x").state["light"] is True

    r = client.post("/node/heartbeat", json={"device_id": "agri_x"})
    assert r.status_code == 200

    r = client.get("/node/command/agri_x")
    assert r.status_code == 200
    assert r.json() == {}

    reg.enqueue_hub_command("agri_x", "on")
    r = client.get("/node/command/agri_x")
    assert r.json() == {"command": "on", "slot": 0}

    r = client.post("/node/register", json={"device_id": "y", "sensor_type": 99})
    assert r.status_code == 400
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd bridge/bridge_python && pytest tests/test_http_node.py -v`
Expected: FAIL (404 em `/node/register`).

- [ ] **Step 3: Write minimal implementation**

In `app/http_api.py`, adicionar import no topo:

```python
from app.hub_compat import (
    handle_node_register,
    handle_node_state,
    handle_node_heartbeat,
    handle_node_command_get,
)
```

Dentro de `create_app`, após as rotas existentes, adicionar:

```python
    @app.post("/node/register")
    async def node_register(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        mqtt = getattr(request.app.state, "mqtt", None)
        ws = request.app.state.ws_manager
        resp, code = await handle_node_register(registry, mqtt, ws, body)
        return JSONResponse(resp, status_code=code)

    @app.post("/node/state")
    async def node_state(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        ws = request.app.state.ws_manager
        resp, code = await handle_node_state(registry, ws, body)
        return JSONResponse(resp, status_code=code)

    @app.post("/node/heartbeat")
    async def node_heartbeat(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        resp, code = await handle_node_heartbeat(registry, body)
        return JSONResponse(resp, status_code=code)

    @app.get("/node/command/{device_id}")
    async def node_command(device_id: str):
        resp, code = await handle_node_command_get(registry, device_id)
        return JSONResponse(resp, status_code=code)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd bridge/bridge_python && pytest tests/test_http_node.py -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/http_api.py tests/test_http_node.py
git commit -m "feat(bridge): add /node/* HTTP routes mirroring hub TCP API"
```

---

### Task 6: Listener MQTT de comandos HA (`main.py`)

**Files:**
- Modify: `bridge/bridge_python/app/main.py` (`hub_command_listener` + `startup`)
- Test: coberto por `tests/test_hub_compat.py` (Task 3: `translate_ha_payload`/`device_id_from_command_topic`).

**Interfaces:**
- Consumes: `translate_ha_payload`, `device_id_from_command_topic` (Task 3), `registry` (global), `settings`.
- Produces: `hub_command_listener()` task; comandos HA→`registry.enqueue_hub_command`.

- [ ] **Step 1: Write the failing test** — já coberto em Task 3 (`test_translate_and_topic`). Pular novo teste; garantir que passa:

Run: `cd bridge/bridge_python && pytest tests/test_hub_compat.py::test_translate_and_topic -v`
Expected: PASS.

- [ ] **Step 2: Write minimal implementation**

In `app/main.py`, adicionar import e a função (após `force_update_listener`):

```python
from app.hub_compat import translate_ha_payload, device_id_from_command_topic
```

```python
async def hub_command_listener():
    import aiomqtt
    while True:
        try:
            async with aiomqtt.Client(
                hostname=settings.mqtt_host,
                port=settings.mqtt_port,
                username=settings.mqtt_user or None,
                password=settings.mqtt_pass or None,
            ) as client:
                await client.subscribe("homeassistant/+/+/+/set")
                async for message in client.messages:
                    topic = str(message.topic)
                    payload = message.payload.decode().strip() if message.payload else ""
                    cmd = translate_ha_payload(payload)
                    if cmd is None:
                        continue
                    device_id = device_id_from_command_topic(topic)
                    if device_id and registry.get_device(device_id):
                        registry.enqueue_hub_command(device_id, cmd)
        except Exception:
            LOG.exception("hub command listener error, retrying in 10s")
            await asyncio.sleep(10)
```

Em `startup()`, adicionar a task (junto das outras `asyncio.create_task`):

```python
    asyncio.create_task(hub_command_listener())
```

- [ ] **Step 3: Run full test suite**

Run: `cd bridge/bridge_python && pytest -v`
Expected: PASS (sem necessidade de broker MQTT real; o listener só é exercitado em runtime).

- [ ] **Step 4: Commit**

```bash
git add app/main.py
git commit -m "feat(bridge): subscribe HA command topics and enqueue hub-compat commands"
```

---

### Task 7: Teste de integração do ciclo do node

**Files:**
- Create: `bridge/bridge_python/tests/test_hub_integration.py`

**Interfaces:**
- Consumes: `hub_compat` handlers (Task 3), `DeviceRegistry` (Task 2), `translate_ha_payload` (Task 3).
- Produces: validação end-to-end do contrato `lamp → bridge` (registro, estado relay, comando on/off).

- [ ] **Step 1: Write the integration test**

```python
# tests/test_hub_integration.py
from unittest.mock import AsyncMock
from app.device_registry import DeviceRegistry
from app.models import DeviceType
from app import hub_compat


async def _cycle(tmp_path, sensor_type, relay_key, relay_val):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    mqtt = AsyncMock()
    ws = AsyncMock()
    did = "agri_lamp1"
    resp, code = await hub_compat.handle_node_register(
        reg, mqtt, ws, {"device_id": did, "sensor_type": sensor_type, "device_name": "Lamp"}
    )
    assert code == 200 and resp["assigned_slot"] == 0
    resp, code = await hub_compat.handle_node_state(
        reg, ws, {"device_id": did, relay_key: relay_val, "ip": "192.168.1.20"}
    )
    assert code == 200
    dev = reg.get_device(did)
    assert dev.ip == "192.168.1.20"
    # HA liga
    cmd = hub_compat.translate_ha_payload("true")
    assert cmd == "on"
    reg.enqueue_hub_command(did, cmd)
    resp, code = await hub_compat.handle_node_command_get(reg, did)
    assert resp == {"command": "on", "slot": 0}
    # consome e fica vazio
    resp, _ = await hub_compat.handle_node_command_get(reg, did)
    assert resp == {}


def test_lamp_light_cycle(tmp_path):
    import asyncio
    asyncio.run(_cycle(tmp_path, 9, "relay_state", True))


def test_onoff_switch_cycle(tmp_path):
    import asyncio
    asyncio.run(_cycle(tmp_path, 8, "state", False))
```

- [ ] **Step 2: Run test to verify it passes**

Run: `cd bridge/bridge_python && pytest tests/test_hub_integration.py -v`
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/test_hub_integration.py
git commit -m "test(bridge): add hub-compat node lifecycle integration test"
```

---

## Self-Review Notes (já aplicadas)
- Cobertura: Tasks 1–7 cobrem todos os componentes da spec (modelo LIGHT, fila, handlers, UDP binário, rotas HTTP, listener MQTT, integração).
- Sem placeholders: todo step tem código/conteúdo exato.
- Consistência de tipos: `enqueue_hub_command`/`get_hub_command`/`slot_of` definidos na Task 2 e usados na Task 3; `build_announce_bytes` definido na Task 3 e usado na Task 4; `translate_ha_payload`/`device_id_from_command_topic` definidos na Task 3 e usados na Task 6. Nomes batem em todos os tasks.
- `bridge_ip` real: `UDPDiscovery` já aceita `bridge_ip` (config) ou auto-detecta; o announce usa `self._bridge_ip`.
