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
