from __future__ import annotations
import asyncio
import logging
import time
from app.config import settings
from app.device_registry import DeviceRegistry
from app.http_api import create_app
from app.host_monitor import HostMonitor
from app.mqtt_discovery import MQTTDiscovery
from app.udp_discovery import UDPDiscovery
from app.websocket_manager import WebSocketManager

LOG = logging.getLogger(__name__)

registry = DeviceRegistry(data_dir=settings.data_dir)
ws_manager = WebSocketManager()
udp = UDPDiscovery(http_port=settings.http_port)
app = create_app(registry, ws_manager, udp)
app.state.ws_manager = ws_manager

mqtt = MQTTDiscovery(
    host=settings.mqtt_host,
    port=settings.mqtt_port,
    user=settings.mqtt_user,
    password=settings.mqtt_pass,
)
host_monitor = HostMonitor(mqtt)


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
    for dev in registry.get_all():
        await mqtt.publish_device_config(dev)
    await udp.start()
    await host_monitor.start()
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
