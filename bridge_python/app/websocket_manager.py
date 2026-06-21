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
