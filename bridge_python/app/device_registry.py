from __future__ import annotations
import json
import logging
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
            logging.warning("Failed to load devices from %s", self._file_path)

    def _get_slot(self, dev: BridgedDevice) -> int:
        for i, d in enumerate(self.get_all()):
            if d.id == dev.id:
                return i
        return -1
