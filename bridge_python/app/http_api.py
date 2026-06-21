from __future__ import annotations
import asyncio
import json
import logging
import os
import time
from fastapi import FastAPI, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse, JSONResponse, Response
from app.device_registry import DeviceRegistry
from app.models import DeviceType
from app.udp_discovery import UDPDiscovery
from app.websocket_manager import WebSocketManager

LOG = logging.getLogger(__name__)

_start_time = time.monotonic()


def _uptime_s() -> int:
    return int(time.monotonic() - _start_time)


def create_app(registry: DeviceRegistry, ws_manager: WebSocketManager | None = None, udp_discovery: UDPDiscovery | None = None) -> FastAPI:
    if ws_manager is None:
        ws_manager = WebSocketManager()
    app = FastAPI(title="ESP32 Bridge Python", version="0.1.0")
    app.state.ws_manager = ws_manager
    if udp_discovery:
        app.state.udp_discovery = udp_discovery

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
            LOG.warning("Registro recusado: %s (limite de %d devices)", device_id, 32)
            return JSONResponse({"status": "error", "message": "registry full"}, status_code=500)
        LOG.info("Device registrado: %s (%s) como %s em %s", device_id, name, device_type.value, ip or "?")
        ws_manager = request.app.state.ws_manager
        await ws_manager.notify_device_registered(registry.get_device(device_id))
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
            LOG.info("Device removido: %s", device_id)
            ws_manager = request.app.state.ws_manager
            await ws_manager.notify_device_removed(device_id)
            return {"status": "ok"}
        LOG.warning("Remocao falhou: device %s nao encontrado", device_id)
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
        state_items = {}
        for key, value in body.items():
            if key == "id":
                continue
            state_items[key] = value
            if registry.update_state(device_id, key, value):
                found = True
        if not found:
            LOG.warning("State update falhou: device %s nao encontrado", device_id)
            return JSONResponse({"status": "error", "message": "device not found"}, status_code=404)
        LOG.info("State recebido de %s: %s", device_id, state_items)
        ws_manager = request.app.state.ws_manager
        dev = registry.get_device(device_id)
        if dev:
            await ws_manager.notify_device_update(dev)
        return {"status": "ok"}

    @app.get("/api/device/commands")
    async def device_commands_get(id: str = ""):
        if not id:
            return JSONResponse({"status": "error", "message": "missing id"}, status_code=400)
        cmds = registry.get_commands(id)
        if cmds:
            LOG.info("Comandos enviados para %s: %s", id, cmds)
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
        if cmds:
            LOG.info("Comandos enviados para %s: %s", device_id, cmds)
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
            "version": "v0.1.0",
            "uptime_s": _uptime_s(),
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
            LOG.warning("Heartbeat de device desconhecido: %s", device_id)
            return JSONResponse({"status": "error", "message": "device not found"}, status_code=404)
        dev.last_seen = time.time()
        dev.online = True
        ws_manager = request.app.state.ws_manager
        await ws_manager.notify_device_online(device_id, True)
        LOG.info("Heartbeat recebido de %s (%s)", device_id, dev.name)
        return {"status": "ok"}

    @app.post("/api/gateway/broadcast")
    async def broadcast(request: Request):
        udp = getattr(request.app.state, "udp_discovery", None)
        if udp:
            udp.do_broadcast()
            LOG.info("Broadcast enviado")
        return {"status": "ok", "message": "broadcast sent"}

    @app.post("/api/gateway/reset")
    async def reset():
        loop = asyncio.get_event_loop()
        loop.call_later(0.5, os._exit, 0)
        return {"status": "ok", "message": "reset initiated"}

    @app.get("/api/qrcode")
    async def qrcode():
        return {"service_name": "esp-bridge", "pop": ""}

    @app.post("/api/ota")
    async def ota():
        return {"status": "ok", "message": "ota not applicable in python"}

    @app.websocket("/ws")
    async def ws_endpoint(websocket: WebSocket):
        await ws_manager.connect(websocket)
        try:
            while True:
                await websocket.receive_text()
        except WebSocketDisconnect:
            ws_manager.disconnect(websocket)

    @app.get("/")
    async def dashboard_html():
        from app.web import dashboard_html_content
        return HTMLResponse(content=dashboard_html_content)

    @app.get("/dashboard.css")
    async def dashboard_css():
        from app.web import dashboard_css_content
        return Response(content=dashboard_css_content, media_type="text/css")

    return app
