# Python Bridge HA Add-on Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) for syntax tracking.

**Goal:** Build a Python clone of the ESP32 bridge as an HA add-on with MQTT Discovery

**Architecture:** FastAPI + asyncio for HTTP/UDP/WebSocket, aiomqtt for MQTT Discovery, JSON file persistence. Single process with all services in the same event loop.

**Tech Stack:** Python 3.11+, FastAPI, uvicorn, aiomqtt, pydantic, pytest, httpx

## Global Constraints

- All new code in `bridge_python/` — no changes to existing ESP32 code
- Same HTTP API endpoints as ESP32 bridge (compatibility with existing ESP8266 clients)
- Same device types (11 types from RainMaker enum)
- MQTT Discovery via existing Mosquitto add-on (`core-mosquitto:1883`)
- UDP discovery on port 5000, service name `"esp-bridge"`
- Dashboard web mantido (HTML/CSS copiado do bridge original)
- Persistência em JSON (`data/devices.json`)

---

### Task 1: Project Scaffold + Models + Config

**Files:**
- Create: `bridge_python/requirements.txt`
- Create: `bridge_python/app/__init__.py`
- Create: `bridge_python/app/models.py`
- Create: `bridge_python/app/config.py`
- Create: `bridge_python/app/main.py` (stub)

**Interfaces:**
- Produces: `DeviceType` enum, `BridgedDevice` dataclass, `Settings` pydantic model, `get_settings()` function

- [ ] **Step 1: Create project structure**

```bash
mkdir -p bridge_python/app/web bridge_python/data bridge_python/tests
touch bridge_python/app/__init__.py bridge_python/tests/__init__.py
```

- [ ] **Step 2: Write requirements.txt**

File: `bridge_python/requirements.txt`
```
fastapi>=0.110.0
uvicorn[standard]>=0.27.0
aiomqtt>=2.0.0
pydantic>=2.0.0
aiofiles>=23.0
pytest>=8.0
httpx>=0.27.0
pytest-asyncio>=0.23.0
```

- [ ] **Step 3: Write models.py**

File: `bridge_python/app/models.py`
```python
from __future__ import annotations
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class DeviceType(str, Enum):
    ONOFF = "onoff"
    DIMMABLE = "dimmable"
    TEMPERATURE = "temperature"
    HUMIDITY = "humidity"
    CONTACT = "contact"
    OCCUPANCY = "occupancy"
    LIGHT_SENSOR = "light_sensor"
    TANQUE = "tanque"
    GAS = "gas"
    RAIN = "rain"
    ELECTRICITY = "electricity"
    UNKNOWN = "unknown"

    @classmethod
    def from_string(cls, s: str) -> DeviceType:
        try:
            return cls(s.lower())
        except ValueError:
            return cls.UNKNOWN


@dataclass
class BridgedDevice:
    id: str
    name: str
    type: DeviceType
    ip: str = ""
    registered: bool = True
    online: bool = False
    last_seen: float = 0.0
    state: dict[str, float | bool | str] = field(default_factory=dict)
    commands: list[dict] = field(default_factory=list)


DEVICE_TYPE_MAP: dict[str, str] = {
    "onoff": "switch",
    "dimmable": "light",
    "temperature": "temperature",
    "humidity": "humidity",
    "contact": "contact",
    "occupancy": "occupancy",
    "light_sensor": "light_sensor",
    "tanque": "tanque",
    "gas": "gas",
    "rain": "rain",
    "electricity": "electricity",
}


REGISTER_REQUEST_KEYS = {"id", "type", "name", "ip"}
```

- [ ] **Step 4: Write config.py**

File: `bridge_python/app/config.py`
```python
from __future__ import annotations
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    mqtt_host: str = "core-mosquitto"
    mqtt_port: int = 1883
    mqtt_user: str = ""
    mqtt_pass: str = ""
    log_level: str = "info"
    http_port: int = 80
    discovery_port: int = 5000
    data_dir: str = "data"

    model_config = {"env_prefix": ""}


settings = Settings()
```

- [ ] **Step 5: Write main.py stub**

File: `bridge_python/app/main.py`
```python
from __future__ import annotations


def main() -> None:
    pass


if __name__ == "__main__":
    main()
```

- [ ] **Step 6: Commit**

```bash
git add bridge_python/
git commit -m "feat: add python bridge scaffold with models and config"
```

---

### Task 2: Device Registry with Persistence

**Files:**
- Create: `bridge_python/app/device_registry.py`
- Create: `bridge_python/tests/test_device_registry.py`

**Interfaces:**
- Consumes: `BridgedDevice`, `DeviceType` from models.py
- Produces: `DeviceRegistry` class with `register()`, `remove()`, `update_state()`, `add_command()`, `get_commands()`, `get_device()`, `get_all()`, `get_by_index()`, `check_heartbeats()`, `save()`, `load()`

- [ ] **Step 1: Write the failing test**

File: `bridge_python/tests/test_device_registry.py`
```python
from __future__ import annotations
import pytest
from app.device_registry import DeviceRegistry
from app.models import DeviceType, BridgedDevice


@pytest.fixture
def registry(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    return reg


class TestDeviceRegistry:
    def test_register_device(self, registry):
        slot = registry.register("esp8266_test", DeviceType.TEMPERATURE, "Sensor Test", "192.168.1.10")
        assert slot == 0
        dev = registry.get_device("esp8266_test")
        assert dev is not None
        assert dev.id == "esp8266_test"
        assert dev.type == DeviceType.TEMPERATURE
        assert dev.name == "Sensor Test"
        assert dev.ip == "192.168.1.10"
        assert dev.registered is True

    def test_register_duplicate_re_registers(self, registry):
        registry.register("esp8266_test", DeviceType.GAS, "Gas", "10.0.0.1")
        slot = registry.register("esp8266_test", DeviceType.GAS, "Gas Renamed", "10.0.0.2")
        assert slot == 0
        dev = registry.get_device("esp8266_test")
        assert dev.name == "Gas Renamed"
        assert dev.ip == "10.0.0.2"

    def test_register_full_registry(self, registry):
        for i in range(32):
            registry.register(f"esp8266_{i}", DeviceType.ONOFF, f"Dev {i}", "")
        slot = registry.register("esp8266_extra", DeviceType.ONOFF, "Extra", "")
        assert slot == -1

    def test_remove_device(self, registry):
        registry.register("esp8266_test", DeviceType.ONOFF, "Test", "")
        assert registry.remove("esp8266_test") is True
        assert registry.get_device("esp8266_test") is None

    def test_remove_nonexistent(self, registry):
        assert registry.remove("nonexistent") is False

    def test_update_state(self, registry):
        registry.register("esp8266_test", DeviceType.TEMPERATURE, "Test", "")
        assert registry.update_state("esp8266_test", "temperature", 23.5) is True
        dev = registry.get_device("esp8266_test")
        assert dev.state["temperature"] == 23.5

    def test_update_state_nonexistent(self, registry):
        assert registry.update_state("nope", "temperature", 1.0) is False

    def test_add_and_get_commands(self, registry):
        registry.register("esp8266_test", DeviceType.ONOFF, "Test", "")
        assert registry.add_command("esp8266_test", "onoff", "set_onoff", "1") is True
        cmds = registry.get_commands("esp8266_test")
        assert len(cmds) == 1
        assert cmds[0]["cluster"] == "onoff"
        assert cmds[0]["command"] == "set_onoff"
        assert cmds[0]["data"] == "1"
        # commands are consumed after get
        assert len(registry.get_commands("esp8266_test")) == 0

    def test_get_all(self, registry):
        registry.register("a", DeviceType.ONOFF, "A", "")
        registry.register("b", DeviceType.GAS, "B", "")
        all_devs = registry.get_all()
        assert len(all_devs) == 2

    def test_get_by_index(self, registry):
        registry.register("a", DeviceType.ONOFF, "A", "")
        dev = registry.get_by_index(0)
        assert dev is not None and dev.id == "a"
        assert registry.get_by_index(99) is None

    def test_check_heartbeats_marks_offline(self, registry):
        import time
        registry.register("esp8266_test", DeviceType.ONOFF, "Test", "")
        registry.get_device("esp8266_test").online = True
        registry.get_device("esp8266_test").last_seen = time.time() - 120
        registry.check_heartbeats(timeout=60)
        assert registry.get_device("esp8266_test").online is False

    def test_persistence(self, tmp_path):
        reg1 = DeviceRegistry(data_dir=str(tmp_path))
        reg1.load()
        reg1.register("persist_test", DeviceType.TEMPERATURE, "Persisted", "10.0.0.1")
        reg1.update_state("persist_test", "temperature", 25.0)
        reg1.save()
        reg2 = DeviceRegistry(data_dir=str(tmp_path))
        reg2.load()
        dev = reg2.get_device("persist_test")
        assert dev is not None
        assert dev.id == "persist_test"
        assert dev.type == DeviceType.TEMPERATURE
        assert dev.name == "Persisted"
        assert dev.ip == "10.0.0.1"
        assert dev.state.get("temperature") == 25.0
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd bridge_python && pip install -q -r requirements.txt && python -m pytest tests/test_device_registry.py -v
```
Expected: FAIL with module not found errors

- [ ] **Step 3: Write minimal implementation**

File: `bridge_python/app/device_registry.py`
```python
from __future__ import annotations
import json
import os
import time
from app.models import BridgedDevice, DeviceType

MAX_DEVICES = 32


class DeviceRegistry:
    def __init__(self, data_dir: str = "data"):
        self._devices: dict[str, BridgedDevice] = {}
        self._data_dir = data_dir
        self._file_path = os.path.join(data_dir, "devices.json")

    def register(
        self, device_id: str, device_type: DeviceType, name: str, ip: str
    ) -> int:
        existing = self._devices.get(device_id)
        if existing:
            existing.name = name
            existing.ip = ip
            if device_type != DeviceType.UNKNOWN:
                existing.type = device_type
            existing.registered = True
            self.save()
            return self._get_slot(existing)
        if len(self._devices) >= MAX_DEVICES:
            return -1
        dev = BridgedDevice(
            id=device_id,
            name=name,
            type=device_type,
            ip=ip,
            registered=True,
            online=True,
            last_seen=time.time(),
        )
        self._devices[device_id] = dev
        self.save()
        return self._get_slot(dev)

    def remove(self, device_id: str) -> bool:
        dev = self._devices.pop(device_id, None)
        if dev:
            self.save()
            return True
        return False

    def update_state(self, device_id: str, key: str, value: float | bool | str) -> bool:
        dev = self._devices.get(device_id)
        if not dev:
            return False
        dev.state[key] = value
        dev.last_seen = time.time()
        dev.online = True
        return True

    def add_command(self, device_id: str, cluster: str, command: str, data: str) -> bool:
        dev = self._devices.get(device_id)
        if not dev:
            return False
        dev.commands.append({"cluster": cluster, "command": command, "data": data})
        return True

    def get_commands(self, device_id: str) -> list[dict]:
        dev = self._devices.get(device_id)
        if not dev:
            return []
        cmds = list(dev.commands)
        dev.commands.clear()
        return cmds

    def get_device(self, device_id: str) -> BridgedDevice | None:
        return self._devices.get(device_id)

    def get_all(self) -> list[BridgedDevice]:
        return list(self._devices.values())

    def get_by_index(self, index: int) -> BridgedDevice | None:
        all_devs = self.get_all()
        if 0 <= index < len(all_devs):
            return all_devs[index]
        return None

    def check_heartbeats(self, timeout: int = 60):
        now = time.time()
        for dev in self._devices.values():
            if dev.online and (now - dev.last_seen) > timeout:
                dev.online = False

    def save(self):
        os.makedirs(self._data_dir, exist_ok=True)
        data = []
        for dev in self._devices.values():
            data.append({
                "id": dev.id,
                "name": dev.name,
                "type": dev.type.value,
                "ip": dev.ip,
                "registered": dev.registered,
                "online": dev.online,
                "last_seen": dev.last_seen,
                "state": dev.state,
                "commands": dev.commands,
            })
        with open(self._file_path, "w") as f:
            json.dump(data, f, indent=2)

    def load(self):
        if not os.path.exists(self._file_path):
            return
        try:
            with open(self._file_path) as f:
                data = json.load(f)
            for item in data:
                dev = BridgedDevice(
                    id=item["id"],
                    name=item.get("name", item["id"]),
                    type=DeviceType.from_string(item.get("type", "unknown")),
                    ip=item.get("ip", ""),
                    registered=item.get("registered", True),
                    online=item.get("online", False),
                    last_seen=item.get("last_seen", 0.0),
                    state=item.get("state", {}),
                    commands=item.get("commands", []),
                )
                self._devices[dev.id] = dev
        except (json.JSONDecodeError, KeyError):
            pass

    def _get_slot(self, dev: BridgedDevice) -> int:
        for i, d in enumerate(self.get_all()):
            if d.id == dev.id:
                return i
        return -1
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd bridge_python && python -m pytest tests/test_device_registry.py -v
```
Expected: All tests PASS

- [ ] **Step 5: Commit**

```bash
git add bridge_python/app/device_registry.py bridge_python/tests/test_device_registry.py
git commit -m "feat: add device registry with JSON persistence"
```

---

### Task 3: HTTP REST API (FastAPI)

**Files:**
- Create: `bridge_python/app/main.py` (full implementation)
- Create: `bridge_python/app/http_api.py`
- Create: `bridge_python/tests/test_http_api.py`

**Interfaces:**
- Consumes: `DeviceRegistry`, `BridgedDevice`, `DeviceType` from previous tasks
- Produces: FastAPI app with all HTTP endpoints (on port 80), `create_app(registry)` factory function

- [ ] **Step 1: Write the failing test**

File: `bridge_python/tests/test_http_api.py`
```python
from __future__ import annotations
import pytest
from httpx import AsyncClient, ASGITransport
from app.http_api import create_app
from app.device_registry import DeviceRegistry


@pytest.fixture
def registry(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    return reg


@pytest.fixture
async def client(registry):
    app = create_app(registry)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac


@pytest.mark.asyncio
class TestHttpAPI:
    async def test_ping(self, client):
        resp = await client.get("/api/ping")
        assert resp.status_code == 200
        assert resp.json() == {"status": "ok"}

    async def test_register_device(self, client):
        resp = await client.post("/api/device/register", json={
            "id": "esp8266_test",
            "type": "temperature",
            "name": "Sensor Test",
            "ip": "192.168.1.10",
        })
        assert resp.status_code == 200
        data = resp.json()
        assert data["status"] == "ok"
        assert "slot" in data

    async def test_register_missing_id(self, client):
        resp = await client.post("/api/device/register", json={"type": "gas"})
        assert resp.status_code == 400

    async def test_register_missing_type(self, client):
        resp = await client.post("/api/device/register", json={"id": "x"})
        assert resp.status_code == 400

    async def test_register_unknown_type(self, client):
        resp = await client.post("/api/device/register", json={
            "id": "x", "type": "invalid_type"
        })
        assert resp.status_code == 400

    async def test_device_state(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_test", "type": "temperature", "name": "T1",
        })
        resp = await client.post("/api/device/state", json={
            "id": "esp8266_test", "temperature": 23.5, "humidity": 65.0,
        })
        assert resp.status_code == 200
        assert resp.json() == {"status": "ok"}

    async def test_device_state_nonexistent(self, client):
        resp = await client.post("/api/device/state", json={
            "id": "nonexistent", "temperature": 23.5,
        })
        assert resp.status_code == 404

    async def test_remove_device(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_test", "type": "gas", "name": "GAS",
        })
        resp = await client.post("/api/device/remove", json={"id": "esp8266_test"})
        assert resp.status_code == 200
        assert resp.json() == {"status": "ok"}

    async def test_remove_nonexistent(self, client):
        resp = await client.post("/api/device/remove", json={"id": "nope"})
        assert resp.status_code == 404

    async def test_devices_list(self, client):
        await client.post("/api/device/register", json={
            "id": "a", "type": "onoff", "name": "A",
        })
        await client.post("/api/device/register", json={
            "id": "b", "type": "gas", "name": "B",
        })
        resp = await client.get("/api/devices")
        assert resp.status_code == 200
        data = resp.json()
        assert len(data) == 2

    async def test_device_info(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_test", "type": "temperature", "name": "T1", "ip": "10.0.0.1",
        })
        resp = await client.get("/api/device/info?id=esp8266_test")
        assert resp.status_code == 200
        data = resp.json()
        assert data["id"] == "esp8266_test"
        assert data["name"] == "T1"

    async def test_device_commands(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_test", "type": "onoff", "name": "SW",
        })
        resp = await client.get("/api/device/commands?id=esp8266_test")
        assert resp.status_code == 200
        data = resp.json()
        assert "commands" in data

    async def test_device_commands_post(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_test", "type": "onoff", "name": "SW",
        })
        resp = await client.post("/api/device/commands", json={"id": "esp8266_test"})
        assert resp.status_code == 200

    async def test_heartbeat(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_test", "type": "onoff", "name": "SW",
        })
        resp = await client.post("/api/device/heartbeat", json={"id": "esp8266_test"})
        assert resp.status_code == 200
        assert resp.json() == {"status": "ok"}

    async def test_gateway_info(self, client):
        resp = await client.get("/api/gateway/info")
        assert resp.status_code == 200
        data = resp.json()
        assert "version" in data
        assert "total_devices" in data

    async def test_broadcast(self, client):
        resp = await client.post("/api/gateway/broadcast")
        assert resp.status_code == 200

    async def test_qrcode(self, client):
        resp = await client.get("/api/qrcode")
        assert resp.status_code == 200
        data = resp.json()
        assert "service_name" in data

    async def test_reset(self, client):
        resp = await client.post("/api/gateway/reset")
        assert resp.status_code == 200

    async def test_ota(self, client):
        resp = await client.post("/api/ota")
        assert resp.status_code == 200
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd bridge_python && python -m pytest tests/test_http_api.py -v
```
Expected: FAIL with import errors

- [ ] **Step 3: Write http_api.py**

File: `bridge_python/app/http_api.py`
```python
from __future__ import annotations
import json
import time
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, JSONResponse, Response
from app.device_registry import DeviceRegistry
from app.models import DeviceType


def create_app(registry: DeviceRegistry) -> FastAPI:
    app = FastAPI(title="ESP32 Bridge Python", version="0.1.0")

    @app.get("/api/ping")
    async def ping():
        return {"status": "ok"}

    @app.post("/api/device/register")
    async def register_device(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        device_id = body.get("id")
        device_type_str = body.get("type")
        if not device_id or not device_type_str:
            return JSONResponse({"status": "error", "message": "missing id or type"}, status_code=400)
        device_type = DeviceType.from_string(device_type_str)
        if device_type == DeviceType.UNKNOWN:
            return JSONResponse({"status": "error", "message": "unknown type"}, status_code=400)
        name = body.get("name", device_id)
        ip = body.get("ip", "")
        slot = registry.register(device_id, device_type, name, ip)
        if slot == -1:
            return JSONResponse({"status": "error", "message": "registry full"}, status_code=500)
        return {"status": "ok", "slot": slot}

    @app.post("/api/device/remove")
    async def remove_device(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        device_id = body.get("id")
        if not device_id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        if registry.remove(device_id):
            return {"status": "ok"}
        return JSONResponse({"status": "error", "message": "device not found"}, status_code=404)

    @app.post("/api/device/state")
    async def device_state(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        device_id = body.get("id")
        if not device_id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        found = False
        for key, value in body.items():
            if key == "id":
                continue
            if registry.update_state(device_id, key, value):
                found = True
        if not found:
            return JSONResponse({"status": "error", "message": "device not found"}, status_code=404)
        return {"status": "ok"}

    @app.get("/api/device/commands")
    async def device_commands_get(id: str = ""):
        if not id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        cmds = registry.get_commands(id)
        return {"commands": cmds}

    @app.post("/api/device/commands")
    async def device_commands_post(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        device_id = body.get("id")
        if not device_id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        cmds = registry.get_commands(device_id)
        return {"commands": cmds}

    @app.get("/api/device/info")
    async def device_info(id: str = ""):
        if not id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        dev = registry.get_device(id)
        if not dev:
            return JSONResponse({"status": "error", "message": "device not found"}, status_code=404)
        return {
            "id": dev.id,
            "name": dev.name,
            "type": dev.type.value,
            "ip": dev.ip,
            "online": dev.online,
            "last_seen": dev.last_seen,
            "state": dev.state,
        }

    @app.get("/api/devices")
    async def devices_list():
        return [
            {
                "id": d.id,
                "name": d.name,
                "type": d.type.value,
                "ip": d.ip,
                "online": d.online,
                "state": d.state,
                "last_seen": d.last_seen,
            }
            for d in registry.get_all()
        ]

    @app.get("/api/gateway/info")
    async def gateway_info():
        import socket
        hostname = socket.gethostname()
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
        except Exception:
            ip = "127.0.0.1"
        return {
            "ip": ip,
            "gateway": "",
            "netmask": "",
            "dns1": "",
            "dns2": "",
            "version": "v0.1.0",
            "uptime_s": int(time.time()),
            "heap_free": 0,
            "total_devices": len(registry.get_all()),
            "hostname": hostname,
        }

    @app.post("/api/device/heartbeat")
    async def device_heartbeat(request: Request):
        try:
            body = await request.json()
        except json.JSONDecodeError:
            return JSONResponse({"status": "error", "message": "invalid json"}, status_code=400)
        device_id = body.get("id")
        if not device_id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        dev = registry.get_device(device_id)
        if not dev:
            return JSONResponse({"status": "error", "message": "device not found"}, status_code=404)
        dev.last_seen = time.time()
        dev.online = True
        return {"status": "ok"}

    @app.post("/api/gateway/broadcast")
    async def broadcast():
        return {"status": "ok", "message": "broadcast sent"}

    @app.post("/api/gateway/reset")
    async def reset():
        return {"status": "ok", "message": "reset initiated"}

    @app.get("/api/qrcode")
    async def qrcode():
        return {"service_name": "esp-bridge", "pop": ""}

    @app.post("/api/ota")
    async def ota():
        return {"status": "ok", "message": "ota not applicable in python"}

    return app
```

- [ ] **Step 4: Update main.py with create_app factory**

File: `bridge_python/app/main.py`
```python
from __future__ import annotations
from app.http_api import create_app
from app.device_registry import DeviceRegistry

registry = DeviceRegistry()
app = create_app(registry)


def main() -> None:
    import uvicorn
    from app.config import settings
    registry.load()
    uvicorn.run(app, host="0.0.0.0", port=settings.http_port, log_level=settings.log_level)


if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cd bridge_python && python -m pytest tests/test_http_api.py -v
```
Expected: All tests PASS

- [ ] **Step 6: Commit**

```bash
git add bridge_python/app/http_api.py bridge_python/app/main.py bridge_python/tests/test_http_api.py
git commit -m "feat: add HTTP REST API with all bridge endpoints"
```

---

### Task 4: Web Dashboard

**Files:**
- Create: `bridge_python/app/web/__init__.py` (empty)
- Create: `bridge_python/app/web/dashboard.html` (copied from `main/web/dashboard.html`)
- Create: `bridge_python/app/web/dashboard.css` (copied from `main/web/dashboard.css`)
- Modify: `bridge_python/app/http_api.py` (add dashboard HTML/CSS routes)

**Interfaces:**
- Consumes: `create_app()` from http_api.py
- Produces: GET `/` and GET `/dashboard.css` routes

- [ ] **Step 1: Copy dashboard files from ESP32 bridge**

```bash
cp main/web/dashboard.html bridge_python/app/web/dashboard.html
cp main/web/dashboard.css bridge_python/app/web/dashboard.css
```

- [ ] **Step 2: Write test for dashboard routes**

Add to `bridge_python/tests/test_http_api.py` in `TestHttpAPI`:
```python
    async def test_dashboard_html(self, client):
        resp = await client.get("/")
        assert resp.status_code == 200
        assert "text/html" in resp.headers["content-type"]

    async def test_dashboard_css(self, client):
        resp = await client.get("/dashboard.css")
        assert resp.status_code == 200
        assert "text/css" in resp.headers["content-type"]
```

- [ ] **Step 3: Add HTML/CSS routes to http_api.py**

Add to `create_app()` in `bridge_python/app/http_api.py`:
```python
    @app.get("/")
    async def dashboard_html():
        from app.web import dashboard_html_content
        return HTMLResponse(content=dashboard_html_content)

    @app.get("/dashboard.css")
    async def dashboard_css():
        from app.web import dashboard_css_content
        return Response(content=dashboard_css_content, media_type="text/css")
```

- [ ] **Step 4: Create web/__init__.py with embedded content**

File: `bridge_python/app/web/__init__.py`
```python
from __future__ import annotations
import os

_dir = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(_dir, "dashboard.html")) as f:
    dashboard_html_content = f.read()

with open(os.path.join(_dir, "dashboard.css")) as f:
    dashboard_css_content = f.read()
```

- [ ] **Step 5: Run tests**

```bash
cd bridge_python && python -m pytest tests/test_http_api.py -v
```
Expected: All tests PASS (including new dashboard tests)

- [ ] **Step 6: Commit**

```bash
git add bridge_python/app/web/
git commit -m "feat: add web dashboard HTML/CSS"
```

---

### Task 5: UDP Discovery

**Files:**
- Create: `bridge_python/app/udp_discovery.py`
- Create: `bridge_python/tests/test_udp_discovery.py`

**Interfaces:**
- Produces: `UDPDiscovery` class with `start()`, `stop()`, `do_broadcast()` methods
- Consumes: none directly (operates independently)

- [ ] **Step 1: Write UDP discovery module**

File: `bridge_python/app/udp_discovery.py`
```python
from __future__ import annotations
import asyncio
import json
import logging
import socket
import time

LOG = logging.getLogger(__name__)

DISCOVERY_PORT = 5000
DISCOVERY_SERVICE = "esp-bridge"
BROADCAST_INTERVAL = 300
BROADCAST_DELAY = 30
MAX_DISCOVERED_IPS = 8
DISCOVERED_IP_TIMEOUT = 300


class UDPDiscovery:
    def __init__(self, bridge_ip: str = "0.0.0.0", http_port: int = 80):
        self._bridge_ip = bridge_ip
        self._http_port = http_port
        self._discovered_ips: dict[str, tuple[str, float]] = {}
        self._running = False
        self._start_time = 0.0

    async def start(self):
        self._running = True
        self._start_time = time.time()
        loop = asyncio.get_event_loop()
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.setblocking(False)
        self._sock.bind(("0.0.0.0", DISCOVERY_PORT))
        LOG.info("UDP discovery listening on port %d", DISCOVERY_PORT)
        self._listener_task = asyncio.create_task(self._listen_loop())
        self._broadcast_task = asyncio.create_task(self._broadcast_loop())

    async def stop(self):
        self._running = False
        self._listener_task.cancel()
        self._broadcast_task.cancel()
        self._sock.close()

    async def _listen_loop(self):
        loop = asyncio.get_event_loop()
        while self._running:
            try:
                data, addr = await loop.sock_recvfrom(self._sock, 1024)
                self._handle_message(data, addr)
            except asyncio.CancelledError:
                break
            except Exception:
                LOG.exception("UDP recv error")

    def _handle_message(self, data: bytes, addr: tuple[str, int]):
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
            self._send_response(addr[0])
        if msg.get("re_register") is True:
            pass

    def _send_response(self, target_ip: str):
        msg = json.dumps({
            "service": DISCOVERY_SERVICE,
            "ip_sta": self._bridge_ip,
            "http_port": self._http_port,
        }).encode()
        try:
            self._sock.sendto(msg, (target_ip, DISCOVERY_PORT))
        except Exception:
            LOG.exception("UDP send error")

    async def _broadcast_loop(self):
        await asyncio.sleep(BROADCAST_DELAY)
        while self._running:
            self._send_broadcast()
            await asyncio.sleep(BROADCAST_INTERVAL)

    def _send_broadcast(self):
        uptime = int(time.time() - self._start_time)
        msg = json.dumps({
            "service": DISCOVERY_SERVICE,
            "ip_sta": self._bridge_ip,
            "http_port": self._http_port,
            "uptime_s": uptime,
            "re_register": True,
        }).encode()
        try:
            self._sock.sendto(msg, ("255.255.255.255", DISCOVERY_PORT))
        except Exception:
            LOG.exception("UDP broadcast error")

    def do_broadcast(self) -> dict:
        self._send_broadcast()
        return {
            "registered": [],
            "discovered": [
                {"id": did, "ip": ip}
                for did, (ip, _) in self._discovered_ips.items()
            ],
        }

    def get_bridge_ip(self) -> str:
        return self._bridge_ip

    def _prune_discovered(self):
        now = time.time()
        to_remove = [
            did for did, (_, ts) in self._discovered_ips.items()
            if now - ts > DISCOVERED_IP_TIMEOUT
        ]
        for did in to_remove:
            del self._discovered_ips[did]
```

- [ ] **Step 2: Write tests**

File: `bridge_python/tests/test_udp_discovery.py`
```python
from __future__ import annotations
import json
import pytest
from app.udp_discovery import UDPDiscovery, DISCOVERY_SERVICE


@pytest.mark.asyncio
class TestUDPDiscovery:
    @pytest.fixture
    def discovery(self):
        ud = UDPDiscovery(bridge_ip="192.168.1.50", http_port=80)
        yield ud

    def test_handle_discover_request(self, discovery):
        data = json.dumps({
            "service": DISCOVERY_SERVICE,
            "discover": True,
            "id": "esp8266_test",
        }).encode()
        discovery._handle_message(data, ("10.0.0.1", 5000))
        assert "esp8266_test" in discovery._discovered_ips
        assert discovery._discovered_ips["esp8266_test"][0] == "10.0.0.1"

    def test_handle_wrong_service(self, discovery):
        data = json.dumps({"service": "wrong"}).encode()
        discovery._handle_message(data, ("10.0.0.1", 5000))
        assert len(discovery._discovered_ips) == 0

    def test_prune_discovered(self, discovery):
        import time
        discovery._discovered_ips["old"] = ("10.0.0.1", time.time() - 600)
        discovery._discovered_ips["new"] = ("10.0.0.2", time.time())
        discovery._prune_discovered()
        assert "old" not in discovery._discovered_ips
        assert "new" in discovery._discovered_ips

    def test_do_broadcast_returns_dict(self, discovery):
        result = discovery.do_broadcast()
        assert "registered" in result
        assert "discovered" in result
```

- [ ] **Step 3: Run tests**

```bash
cd bridge_python && python -m pytest tests/test_udp_discovery.py -v
```
Expected: All tests PASS

- [ ] **Step 4: Commit**

```bash
git add bridge_python/app/udp_discovery.py bridge_python/tests/test_udp_discovery.py
git commit -m "feat: add UDP discovery listener and broadcast"
```

---

### Task 6: MQTT Discovery

**Files:**
- Create: `bridge_python/app/mqtt_discovery.py`
- Create: `bridge_python/tests/test_mqtt_discovery.py`

**Interfaces:**
- Consumes: `DeviceRegistry`, `BridgedDevice`, `DeviceType` from registry
- Produces: `MQTTDiscovery` class with `start()`, `stop()`, `publish_device_config()`, `remove_device_config()`, `publish_state()`

- [ ] **Step 1: Write the failing test**

File: `bridge_python/tests/test_mqtt_discovery.py`
```python
from __future__ import annotations
import json
import pytest
from app.mqtt_discovery import MQTTDiscovery, build_device_info, build_entity_config
from app.models import DeviceType, BridgedDevice


@pytest.fixture
def mqtt():
    m = MQTTDiscovery(host="test", port=1883, user="", password="")
    return m


class TestMQTTDiscovery:
    def test_build_device_info(self):
        dev = BridgedDevice(id="esp8266_test", name="Test", type=DeviceType.TEMPERATURE)
        info = build_device_info(dev)
        assert info["identifiers"] == ["esp32_bridge_esp8266_test"]
        assert info["name"] == "Test"
        assert info["model"] == "temperature"

    def test_build_entity_config_sensor(self):
        config = build_entity_config(
            device_id="esp8266_test",
            platform="sensor",
            entity_name="temperature",
            dev_name="Test",
            unit="°C",
            device_class="temperature",
        )
        topic = config["~"]
        assert config["platform"] == "sensor"
        assert config["unit_of_measurement"] == "°C"
        assert config["device_class"] == "temperature"
        assert config["state_topic"] == f"{topic}/state"
        assert config["name"] == "temperature"

    def test_build_entity_config_switch(self):
        config = build_entity_config(
            device_id="esp8266_test",
            platform="switch",
            entity_name="power",
            dev_name="Test",
        )
        assert config["platform"] == "switch"
        assert config["command_topic"] is not None

    def test_build_topic(self):
        m = MQTTDiscovery(host="h", port=1883)
        topic = m._build_topic("sensor", "esp8266_test", "temperature")
        assert topic == "homeassistant/sensor/esp8266_test/temperature/config"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd bridge_python && python -m pytest tests/test_mqtt_discovery.py -v
```
Expected: FAIL with module not found

- [ ] **Step 3: Write mqtt_discovery.py**

File: `bridge_python/app/mqtt_discovery.py`
```python
from __future__ import annotations
import json
import logging
from typing import Optional
from app.models import BridgedDevice, DeviceType

LOG = logging.getLogger(__name__)

DISCOVERY_PREFIX = "homeassistant"

# Maps device type to list of (entity_name, platform, unit, device_class, icon)
DEVICE_ENTITY_MAP: dict[DeviceType, list[tuple[str, str, str, str, str]]] = {
    DeviceType.ONOFF: [("power", "switch", "", "", "")],
    DeviceType.DIMMABLE: [("light", "light", "", "", "")],
    DeviceType.TEMPERATURE: [
        ("temperature", "sensor", "°C", "temperature", ""),
        ("humidity", "sensor", "%", "humidity", ""),
    ],
    DeviceType.HUMIDITY: [("humidity", "sensor", "%", "humidity", "")],
    DeviceType.CONTACT: [("contact", "binary_sensor", "", "door", "")],
    DeviceType.OCCUPANCY: [("occupancy", "binary_sensor", "", "occupancy", "")],
    DeviceType.LIGHT_SENSOR: [("light", "sensor", "lx", "", "mdi:brightness-5")],
    DeviceType.TANQUE: [("level", "sensor", "%", "", "mdi:water")],
    DeviceType.GAS: [
        ("alarm", "binary_sensor", "", "gas", ""),
        ("gas_level", "sensor", "%", "", "mdi:gas-cylinder"),
    ],
    DeviceType.RAIN: [
        ("rain_digital", "binary_sensor", "", "moisture", ""),
        ("rain_level", "sensor", "%", "", "mdi:weather-rainy"),
    ],
    DeviceType.ELECTRICITY: [("current", "sensor", "mA", "current", "")],
}


def build_device_info(dev: BridgedDevice) -> dict:
    return {
        "identifiers": [f"esp32_bridge_{dev.id}"],
        "name": dev.name,
        "sw_version": "bridge_python_0.1.0",
        "manufacturer": "ESP RainMaker Gateway",
        "model": dev.type.value,
        "via_device": "esp32_bridge",
    }


def build_entity_config(
    device_id: str,
    platform: str,
    entity_name: str,
    dev_name: str,
    unit: str = "",
    device_class: str = "",
    icon: str = "",
) -> dict:
    base_topic = f"{DISCOVERY_PREFIX}/{platform}/{device_id}/{entity_name}"
    config: dict = {
        "~": base_topic,
        "platform": platform,
        "name": entity_name,
        "state_topic": f"{base_topic}/state",
        "unique_id": f"esp32_bridge_{device_id}_{entity_name}",
        "device": {
            "identifiers": [f"esp32_bridge_{device_id}"],
            "name": dev_name,
            "sw_version": "bridge_python_0.1.0",
            "manufacturer": "ESP RainMaker Gateway",
            "model": "bridge_device",
            "via_device": "esp32_bridge",
        },
    }
    if unit:
        config["unit_of_measurement"] = unit
    if device_class:
        config["device_class"] = device_class
    if icon:
        config["icon"] = icon
    if platform in ("switch", "light"):
        config["command_topic"] = f"{base_topic}/set"
        config["payload_on"] = "true"
        config["payload_off"] = "false"
        config["state_on"] = "true"
        config["state_off"] = "false"
    return config


class MQTTDiscovery:
    def __init__(
        self,
        host: str = "core-mosquitto",
        port: int = 1883,
        user: str = "",
        password: str = "",
    ):
        self._host = host
        self._port = port
        self._user = user
        self._password = password
        self._client = None
        self._connected = False

    async def start(self):
        import aiomqtt
        try:
            self._client = aiomqtt.Client(
                hostname=self._host,
                port=self._port,
                username=self._user or None,
                password=self._password or None,
            )
            await self._client.__aenter__()
            self._connected = True
            LOG.info("MQTT connected to %s:%d", self._host, self._port)
        except Exception:
            LOG.warning("MQTT connection failed (will retry on device events)")
            self._connected = False

    async def stop(self):
        if self._client:
            await self._client.__aexit__(None, None, None)

    async def publish_device_config(self, dev: BridgedDevice):
        if not self._connected:
            LOG.warning("MQTT not connected, skipping discovery for %s", dev.id)
            return
        entities = DEVICE_ENTITY_MAP.get(dev.type, [])
        for entity_name, platform, unit, device_class, icon in entities:
            config = build_entity_config(
                device_id=dev.id,
                platform=platform,
                entity_name=entity_name,
                dev_name=dev.name,
                unit=unit,
                device_class=device_class,
                icon=icon,
            )
            topic = self._build_topic(platform, dev.id, entity_name)
            await self._publish(topic, json.dumps(config), retain=True)
        LOG.info("Published MQTT discovery for %s", dev.id)

    async def remove_device_config(self, dev_id: str, dev_type: DeviceType):
        if not self._connected:
            return
        entities = DEVICE_ENTITY_MAP.get(dev_type, [])
        for entity_name, platform, _, _, _ in entities:
            topic = self._build_topic(platform, dev_id, entity_name)
            await self._publish(topic, "", retain=True)
        LOG.info("Removed MQTT discovery for %s", dev_id)

    async def publish_state(self, dev: BridgedDevice):
        if not self._connected:
            return
        entities = DEVICE_ENTITY_MAP.get(dev.type, [])
        for entity_name, platform, _, _, _ in entities:
            value = dev.state.get(entity_name)
            if value is None:
                continue
            topic = self._build_state_topic(platform, dev.id, entity_name)
            # Convert bool to "true"/"false" string for MQTT
            if isinstance(value, bool):
                str_value = "true" if value else "false"
            elif isinstance(value, float):
                str_value = f"{value:.1f}"
            else:
                str_value = str(value)
            await self._publish(topic, str_value)

    async def _publish(self, topic: str, payload: str, retain: bool = False):
        if not self._client:
            return
        try:
            await self._client.publish(topic, payload=payload, retain=retain)
        except Exception:
            LOG.exception("MQTT publish failed for %s", topic)

    def _build_topic(self, platform: str, device_id: str, entity: str) -> str:
        return f"{DISCOVERY_PREFIX}/{platform}/{device_id}/{entity}/config"

    def _build_state_topic(self, platform: str, device_id: str, entity: str) -> str:
        return f"{DISCOVERY_PREFIX}/{platform}/{device_id}/{entity}/state"
```

- [ ] **Step 4: Run tests**

```bash
cd bridge_python && python -m pytest tests/test_mqtt_discovery.py -v
```
Expected: All tests PASS

- [ ] **Step 5: Commit**

```bash
git add bridge_python/app/mqtt_discovery.py bridge_python/tests/test_mqtt_discovery.py
git commit -m "feat: add MQTT Discovery for HA auto-configuration"
```

---

### Task 7: WebSocket Manager + Main Integration

**Files:**
- Create: `bridge_python/app/websocket_manager.py`
- Create: `bridge_python/tests/test_websocket.py`
- Modify: `bridge_python/app/http_api.py` (add WS route)
- Modify: `bridge_python/app/main.py` (wire UDP + MQTT + WS)

**Interfaces:**
- Consumes: `DeviceRegistry`, `UDPDiscovery`, `MQTTDiscovery`
- Produces: `WebSocketManager` class, fully integrated `main()` with all services

- [ ] **Step 1: Write websocket_manager.py**

File: `bridge_python/app/websocket_manager.py`
```python
from __future__ import annotations
import json
import logging
from fastapi import WebSocket
from app.models import BridgedDevice

LOG = logging.getLogger(__name__)


class WebSocketManager:
    def __init__(self):
        self._connections: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self._connections.append(ws)
        LOG.info("WebSocket client connected (%d total)", len(self._connections))

    def disconnect(self, ws: WebSocket):
        if ws in self._connections:
            self._connections.remove(ws)
        LOG.info("WebSocket client disconnected (%d remaining)", len(self._connections))

    async def broadcast(self, message: dict):
        payload = json.dumps(message)
        stale = []
        for ws in self._connections:
            try:
                await ws.send_text(payload)
            except Exception:
                stale.append(ws)
        for ws in stale:
            self.disconnect(ws)

    async def notify_device_update(self, dev: BridgedDevice):
        await self.broadcast({
            "type": "device_update",
            "device": {
                "id": dev.id,
                "name": dev.name,
                "type": dev.type.value,
                "online": dev.online,
                "state": dev.state,
            },
        })

    async def notify_device_online(self, device_id: str, online: bool):
        await self.broadcast({
            "type": "device_online" if online else "device_offline",
            "device": device_id,
        })

    async def notify_device_registered(self, dev: BridgedDevice):
        await self.broadcast({
            "type": "device_registered",
            "device": {
                "id": dev.id,
                "name": dev.name,
                "type": dev.type.value,
            },
        })

    async def notify_device_removed(self, device_id: str):
        await self.broadcast({
            "type": "device_removed",
            "device": device_id,
        })
```

- [ ] **Step 2: Add WebSocket route to http_api.py and wire notifications**

Modify `create_app()` in `bridge_python/app/http_api.py` to accept `ws_manager` parameter:

```python
from app.websocket_manager import WebSocketManager
from fastapi import WebSocket, WebSocketDisconnect

def create_app(registry: DeviceRegistry, ws_manager: WebSocketManager | None = None) -> FastAPI:
    if ws_manager is None:
        ws_manager = WebSocketManager()
```

Add to create_app before return:
```python
    @app.websocket("/ws")
    async def ws_endpoint(websocket: WebSocket):
        await ws_manager.connect(websocket)
        try:
            while True:
                await websocket.receive_text()
        except WebSocketDisconnect:
            ws_manager.disconnect(websocket)
```

Modify `register_device` to notify:
```python
        ws_manager = request.app.state.ws_manager
        ...
        await ws_manager.notify_device_registered(registry.get_device(device_id))
```

Modify `remove_device` to notify:
```python
        await ws_manager.notify_device_removed(device_id)
```

Modify `device_state` to notify after state update:
```python
        dev = registry.get_device(device_id)
        if dev:
            await ws_manager.notify_device_update(dev)
```

Similar for heartbeat.

- [ ] **Step 3: Write test**

File: `bridge_python/tests/test_websocket.py`
```python
from __future__ import annotations
import pytest
from httpx import AsyncClient, ASGITransport
from app.http_api import create_app
from app.device_registry import DeviceRegistry
from app.websocket_manager import WebSocketManager


@pytest.mark.asyncio
class TestWebSocket:
    @pytest.fixture
    async def app(self):
        reg = DeviceRegistry(data_dir="/tmp")
        reg.load()
        ws = WebSocketManager()
        app = create_app(reg, ws)
        return app

    async def test_websocket_connect(self, app):
        transport = ASGITransport(app=app)
        async with AsyncClient(transport=transport, base_url="http://test") as client:
            async with client.websocket_connect("/ws") as ws:
                data = await ws.receive_text()
                # Should not fail on connect
                pass
```

- [ ] **Step 4: Update main.py with full integration**

File: `bridge_python/app/main.py`
```python
from __future__ import annotations
import asyncio
import logging
import time
from app.config import settings
from app.device_registry import DeviceRegistry
from app.http_api import create_app
from app.mqtt_discovery import MQTTDiscovery
from app.udp_discovery import UDPDiscovery
from app.websocket_manager import WebSocketManager

LOG = logging.getLogger(__name__)

registry = DeviceRegistry(data_dir=settings.data_dir)
ws_manager = WebSocketManager()
app = create_app(registry, ws_manager)
app.state.ws_manager = ws_manager

udp = UDPDiscovery(http_port=settings.http_port)
mqtt = MQTTDiscovery(
    host=settings.mqtt_host,
    port=settings.mqtt_port,
    user=settings.mqtt_user,
    password=settings.mqtt_pass,
)


async def heartbeat_monitor():
    while True:
        await asyncio.sleep(30)
        before = {d.id: d.online for d in registry.get_all()}
        registry.check_heartbeats(timeout=60)
        for dev in registry.get_all():
            if before.get(dev.id, False) != dev.online:
                await ws_manager.notify_device_online(dev.id, dev.online)


async def mqtt_state_sync():
    while True:
        await asyncio.sleep(5)
        for dev in registry.get_all():
            if dev.online:
                await mqtt.publish_state(dev)


async def startup():
    registry.load()
    LOG.info("Loaded %d devices from persistence", len(registry.get_all()))
    # Publish MQTT discovery for restored devices
    for dev in registry.get_all():
        await mqtt.publish_device_config(dev)
    await udp.start()
    asyncio.create_task(heartbeat_monitor())
    asyncio.create_task(mqtt_state_sync())


@app.on_event("startup")
async def on_startup():
    await mqtt.start()
    await startup()


def main() -> None:
    import uvicorn
    logging.basicConfig(
        level=getattr(logging, settings.log_level.upper(), logging.INFO),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )
    uvicorn.run(app, host="0.0.0.0", port=settings.http_port, log_level=settings.log_level)


if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Run all tests**

```bash
cd bridge_python && python -m pytest tests/ -v
```
Expected: All tests PASS

- [ ] **Step 6: Commit**

```bash
git add bridge_python/app/websocket_manager.py bridge_python/app/main.py bridge_python/app/http_api.py bridge_python/tests/test_websocket.py
git commit -m "feat: add WebSocket, integrate all services in main"
```

---

### Task 8: HA Add-on Configuration

**Files:**
- Create: `bridge_python/Dockerfile`
- Create: `bridge_python/config.yaml`
- Create: `bridge_python/run.sh`

- [ ] **Step 1: Write Dockerfile**

File: `bridge_python/Dockerfile`
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

- [ ] **Step 2: Write config.yaml**

File: `bridge_python/config.yaml`
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
  80/tcp: "HTTP API + Web Dashboard"
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

- [ ] **Step 3: Write run.sh**

File: `bridge_python/run.sh`
```bash
#!/usr/bin/ash
cd /app
python -m app.main
```

- [ ] **Step 4: Make run.sh executable**

```bash
chmod +x bridge_python/run.sh
```

- [ ] **Step 5: Run final test suite**

```bash
cd bridge_python && python -m pytest tests/ -v
```
Expected: All tests PASS

- [ ] **Step 6: Commit**

```bash
git add bridge_python/Dockerfile bridge_python/config.yaml bridge_python/run.sh
git commit -m "feat: add HA add-on config (Dockerfile, config.yaml, run.sh)"
```
