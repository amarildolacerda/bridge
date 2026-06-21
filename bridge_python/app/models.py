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

