from __future__ import annotations
from fastapi.testclient import TestClient
from app.http_api import create_app
from app.device_registry import DeviceRegistry
from app.websocket_manager import WebSocketManager


class TestWebSocket:
    def test_websocket_connect(self, tmp_path):
        reg = DeviceRegistry(data_dir=str(tmp_path))
        reg.load()
        ws = WebSocketManager()
        app = create_app(reg, ws)
        client = TestClient(app)
        with client.websocket_connect("/ws") as ws_client:
            ws_client.send_text("hello")
