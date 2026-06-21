from __future__ import annotations
import asyncio
import json
import logging
import os
import time

LOG = logging.getLogger(__name__)

DISCOVERY_PREFIX = "homeassistant"
AVAILABILITY_TOPIC = "esp32-bridge/host/availability"
DEVICE_ID = "host"
DEVICE_NAME = "Bridge Host"
SENSORS = [
    ("cpu_usage", "sensor", "%", "mdi:cpu-64-bit"),
    ("memory_usage", "sensor", "%", "mdi:memory"),
    ("disk_usage", "sensor", "%", "mdi:harddisk"),
    ("uptime_days", "sensor", "d", "mdi:clock-outline"),
]

_prev_cpu: tuple[int, ...] | None = None


def _read_cpu() -> tuple[int, ...] | None:
    try:
        with open("/proc/stat") as f:
            line = f.readline()
        parts = line.strip().split()
        if parts[0] != "cpu":
            return None
        return tuple(int(v) for v in parts[1:])
    except (OSError, IndexError, ValueError):
        return None


def _cpu_percent() -> float:
    global _prev_cpu
    cur = _read_cpu()
    if cur is None or len(cur) < 4:
        return 0.0
    if _prev_cpu is not None:
        prev_idle = _prev_cpu[3] + sum(_prev_cpu[4:])
        cur_idle = cur[3] + sum(cur[4:])
        prev_total = sum(_prev_cpu)
        cur_total = sum(cur)
        diff_total = cur_total - prev_total
        diff_idle = cur_idle - prev_idle
        if diff_total > 0:
            return round((1 - diff_idle / diff_total) * 100, 1)
    _prev_cpu = cur
    return 0.0


def _memory_percent() -> float:
    try:
        with open("/proc/meminfo") as f:
            data = f.read()
        lines = data.splitlines()
        mem_total = mem_avail = None
        for line in lines:
            if line.startswith("MemTotal:"):
                mem_total = int(line.split()[1])
            elif line.startswith("MemAvailable:"):
                mem_avail = int(line.split()[1])
        if mem_total and mem_avail:
            return round((1 - mem_avail / mem_total) * 100, 1)
    except (OSError, IndexError, ValueError):
        pass
    return 0.0


def _disk_percent() -> float:
    try:
        st = os.statvfs("/")
        total = st.f_frsize * st.f_blocks
        free = st.f_frsize * st.f_bfree
        if total > 0:
            return round((1 - free / total) * 100, 1)
    except OSError:
        pass
    return 0.0


def _uptime_days() -> float:
    try:
        with open("/proc/uptime") as f:
            uptime_sec = float(f.read().split()[0])
        return round(uptime_sec / 86400, 1)
    except (OSError, IndexError, ValueError):
        return 0.0


def _build_device_info() -> dict:
    return {
        "identifiers": [f"bridge_python_{DEVICE_ID}"],
        "name": DEVICE_NAME,
        "sw_version": "bridge_python_v0.0.9",
        "manufacturer": "ESP-HA Bridge",
        "model": "host",
    }


def _build_config(entity_name: str, platform: str, unit: str, icon: str) -> dict:
    base = f"{DISCOVERY_PREFIX}/{platform}/{DEVICE_ID}/{entity_name}"
    config = {
        "~": base,
        "platform": platform,
        "name": entity_name,
        "state_topic": f"{base}/state",
        "unique_id": f"bridge_python_{DEVICE_ID}_{entity_name}",
        "device": _build_device_info(),
        "availability_topic": AVAILABILITY_TOPIC,
        "payload_available": "online",
        "payload_not_available": "offline",
    }
    if unit:
        config["unit_of_measurement"] = unit
    if icon:
        config["icon"] = icon
    return config


class HostMonitor:
    def __init__(self, mqtt_discovery):
        self._mqtt = mqtt_discovery
        self._task: asyncio.Task | None = None

    async def publish_discovery(self):
        client = self._mqtt._client
        if not client:
            LOG.warning("MQTT not connected, skipping host discovery")
            return
        for entity_name, platform, unit, icon in SENSORS:
            config = _build_config(entity_name, platform, unit, icon)
            topic = f"{DISCOVERY_PREFIX}/{platform}/{DEVICE_ID}/{entity_name}/config"
            await self._mqtt._publish(topic, json.dumps(config), retain=True)
        await self._publish_availability("online")
        LOG.info("Published MQTT discovery for host")

    async def _publish_availability(self, state: str):
        await self._mqtt._publish(AVAILABILITY_TOPIC, state, retain=True)

    async def collect_and_publish(self):
        client = self._mqtt._client
        if not client:
            LOG.warning("host monitor: MQTT not connected")
            return
        values = {
            "cpu_usage": _cpu_percent(),
            "memory_usage": _memory_percent(),
            "disk_usage": _disk_percent(),
            "uptime_days": _uptime_days(),
        }
        for entity_name, platform, _, icon in SENSORS:
            value = values[entity_name]
            topic = f"{DISCOVERY_PREFIX}/{platform}/{DEVICE_ID}/{entity_name}/state"
            str_value = f"{value:.1f}" if isinstance(value, float) else str(value)
            await self._mqtt._publish(topic, str_value, retain=True)
        await self._publish_availability("online")

    async def start(self, interval: int = 60):
        await self.publish_discovery()
        await self.collect_and_publish()

        async def _loop():
            while True:
                await asyncio.sleep(interval)
                try:
                    await self.collect_and_publish()
                except Exception:
                    LOG.exception("host monitor collect error")

        self._task = asyncio.create_task(_loop())

    async def stop(self):
        if self._task:
            self._task.cancel()
            self._task = None
