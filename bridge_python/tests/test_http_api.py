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

    async def test_dashboard_html(self, client):
        resp = await client.get("/")
        assert resp.status_code == 200
        assert "text/html" in resp.headers["content-type"]

    async def test_dashboard_css(self, client):
        resp = await client.get("/dashboard.css")
        assert resp.status_code == 200
        assert "text/css" in resp.headers["content-type"]

    async def test_ha_command_flow(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_sw", "type": "onoff", "name": "Switch",
        })
        resp = await client.post("/api/device/commands", json={"id": "esp8266_sw"})
        assert resp.status_code == 200
        data = resp.json()
        assert "commands" in data
        assert isinstance(data["commands"], list)

    async def test_ha_command_get_after_state(self, client):
        await client.post("/api/device/register", json={
            "id": "esp8266_sw", "type": "onoff", "name": "Switch",
        })
        await client.post("/api/device/state", json={
            "id": "esp8266_sw", "on": True,
        })
        resp = await client.get("/api/device/commands?id=esp8266_sw")
        assert resp.status_code == 200
        data = resp.json()
        assert "commands" in data
        assert data["commands"] == []

    async def test_gateway_info_fields(self, client):
        resp = await client.get("/api/gateway/info")
        assert resp.status_code == 200
        data = resp.json()
        assert "ip" in data
        assert data["ip"]
        assert "version" in data
        assert "uptime_s" in data
        assert data["uptime_s"] >= 0
        assert "total_devices" in data
        assert data["total_devices"] >= 0
        assert "hostname" in data
        # removed fields should not be present
        assert "gateway" not in data
        assert "netmask" not in data
        assert "dns1" not in data
        assert "dns2" not in data
        assert "heap_free" not in data

    async def test_device_info_nonexistent(self, client):
        resp = await client.get("/api/device/info?id=nonexistent")
        assert resp.status_code == 404

    async def test_device_info_missing_id(self, client):
        resp = await client.get("/api/device/info")
        assert resp.status_code == 400
