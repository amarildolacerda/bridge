from __future__ import annotations
import json
from unittest.mock import AsyncMock
import pytest
from app.mqtt_discovery import MQTTDiscovery, build_device_info, build_entity_config, DEVICE_ENTITY_MAP
from app.models import DeviceType, BridgedDevice


@pytest.fixture
def mqtt():
    m = MQTTDiscovery(host="test", port=1883)
    m._connected = True
    m._client = AsyncMock()
    return m


@pytest.fixture
def connected_mqtt():
    m = MQTTDiscovery(host="test", port=1883)
    m._connected = True
    m._client = AsyncMock()
    return m


class TestMQTTDiscovery:
    def test_build_device_info(self):
        dev = BridgedDevice(id="esp8266_test", name="Test", type=DeviceType.TEMPERATURE)
        info = build_device_info(dev)
        assert info["identifiers"] == ["esp32_bridge_esp8266_test"]
        assert info["name"] == "Test"
        assert info["model"] == "temperature"

    def test_build_entity_config_sensor(self):
        dev = BridgedDevice(id="esp8266_test", name="Test", type=DeviceType.TEMPERATURE)
        config = build_entity_config(
            device_id=dev.id,
            platform="sensor",
            entity_name="temperature",
            dev=dev,
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
        dev = BridgedDevice(id="esp8266_test", name="Test", type=DeviceType.ONOFF)
        config = build_entity_config(
            device_id=dev.id,
            platform="switch",
            entity_name="power",
            dev=dev,
        )
        assert config["platform"] == "switch"
        assert config["command_topic"] is not None
        assert config["payload_on"] == "true"
        assert config["payload_off"] == "false"

    def test_build_topic(self):
        m = MQTTDiscovery(host="h", port=1883)
        topic = m._build_topic("sensor", "esp8266_test", "temperature")
        assert topic == "homeassistant/sensor/esp8266_test/temperature/config"

    @pytest.mark.asyncio
    async def test_publish_state_bool_true(self, connected_mqtt):
        dev = BridgedDevice(id="sw1", name="Switch", type=DeviceType.ONOFF, state={"power": True})
        await connected_mqtt.publish_state(dev)
        connected_mqtt._client.publish.assert_called_once()
        _, kwargs = connected_mqtt._client.publish.call_args
        assert kwargs["payload"] == "true"

    @pytest.mark.asyncio
    async def test_publish_state_bool_false(self, connected_mqtt):
        dev = BridgedDevice(id="sw1", name="Switch", type=DeviceType.ONOFF, state={"power": False})
        await connected_mqtt.publish_state(dev)
        _, kwargs = connected_mqtt._client.publish.call_args
        assert kwargs["payload"] == "false"

    @pytest.mark.asyncio
    async def test_publish_state_float(self, connected_mqtt):
        dev = BridgedDevice(id="t1", name="Temp", type=DeviceType.TEMPERATURE, state={"temperature": 23.5})
        await connected_mqtt.publish_state(dev)
        # temperature = 23.5 → formatted as "23.5"
        found = False
        for call in connected_mqtt._client.publish.call_args_list:
            _, kwargs = call
            if kwargs["payload"] == "23.5":
                found = True
        assert found, "temperature state not published"

    @pytest.mark.asyncio
    async def test_publish_state_float_formatted(self, connected_mqtt):
        dev = BridgedDevice(id="t1", name="Temp", type=DeviceType.TEMPERATURE, state={"temperature": 23.567})
        await connected_mqtt.publish_state(dev)
        _, kwargs = connected_mqtt._client.publish.call_args
        assert kwargs["payload"] == "23.6"

    @pytest.mark.asyncio
    async def test_publish_state_none_skipped(self, connected_mqtt):
        dev = BridgedDevice(id="t1", name="Temp", type=DeviceType.TEMPERATURE, state={"temperature": None})
        await connected_mqtt.publish_state(dev)
        connected_mqtt._client.publish.assert_not_called()

    @pytest.mark.asyncio
    async def test_publish_state_multi_entity_gas(self, connected_mqtt):
        dev = BridgedDevice(
            id="gas1", name="Gas",
            type=DeviceType.GAS,
            state={"alarm": True, "gas_level": 75.0},
        )
        await connected_mqtt.publish_state(dev)
        assert connected_mqtt._client.publish.call_count == 2
        payloads = {kwargs["payload"] for _, kwargs in connected_mqtt._client.publish.call_args_list}
        assert "true" in payloads
        assert "75.0" in payloads

    @pytest.mark.asyncio
    async def test_publish_state_multi_entity_rain(self, connected_mqtt):
        dev = BridgedDevice(
            id="rain1", name="Rain",
            type=DeviceType.RAIN,
            state={"rain_digital": False, "rain_level": 30.0},
        )
        await connected_mqtt.publish_state(dev)
        assert connected_mqtt._client.publish.call_count == 2
        payloads = {kwargs["payload"] for _, kwargs in connected_mqtt._client.publish.call_args_list}
        assert "false" in payloads
        assert "30.0" in payloads

    @pytest.mark.asyncio
    async def test_publish_state_int_value(self, connected_mqtt):
        dev = BridgedDevice(id="d1", name="Dimmer", type=DeviceType.DIMMABLE, state={"light": 50})
        await connected_mqtt.publish_state(dev)
        _, kwargs = connected_mqtt._client.publish.call_args
        assert kwargs["payload"] == "50"

    @pytest.mark.asyncio
    async def test_publish_state_not_connected(self):
        m = MQTTDiscovery(host="test", port=1883)
        m._connected = False
        m._client = AsyncMock()
        dev = BridgedDevice(id="t1", name="Test", type=DeviceType.ONOFF, state={"power": True})
        await m.publish_state(dev)
        m._client.publish.assert_not_called()

    @pytest.mark.asyncio
    async def test_publish_device_config(self, connected_mqtt):
        dev = BridgedDevice(id="esp8266_test", name="Test", type=DeviceType.TEMPERATURE)
        await connected_mqtt.publish_device_config(dev)
        # temperature + humidity = 2 entities
        assert connected_mqtt._client.publish.call_count == 2
        for call in connected_mqtt._client.publish.call_args_list:
            args, kwargs = call
            topic = args[0]
            assert topic.startswith("homeassistant/")
            assert topic.endswith("/config")
            assert kwargs["retain"] is True
            payload = json.loads(kwargs["payload"])
            assert "device" in payload
            assert payload["device"]["identifiers"] == ["esp32_bridge_esp8266_test"]

    @pytest.mark.asyncio
    async def test_publish_device_config_switch_has_command_topic(self, connected_mqtt):
        dev = BridgedDevice(id="sw1", name="Switch", type=DeviceType.ONOFF)
        await connected_mqtt.publish_device_config(dev)
        _, kwargs = connected_mqtt._client.publish.call_args
        payload = json.loads(kwargs["payload"])
        assert "command_topic" in payload
        assert payload["command_topic"].endswith("/set")

    @pytest.mark.asyncio
    async def test_publish_device_config_not_connected(self):
        m = MQTTDiscovery(host="test", port=1883)
        m._connected = False
        m._client = AsyncMock()
        dev = BridgedDevice(id="t1", name="Test", type=DeviceType.TEMPERATURE)
        await m.publish_device_config(dev)
        m._client.publish.assert_not_called()

    @pytest.mark.asyncio
    async def test_remove_device_config(self, connected_mqtt):
        await connected_mqtt.remove_device_config("esp8266_test", DeviceType.TEMPERATURE)
        # temperature + humidity = 2 entities to remove
        assert connected_mqtt._client.publish.call_count == 2
        for call in connected_mqtt._client.publish.call_args_list:
            _, kwargs = call
            assert kwargs["payload"] == ""
            assert kwargs["retain"] is True

    @pytest.mark.asyncio
    async def test_remove_device_config_not_connected(self):
        m = MQTTDiscovery(host="test", port=1883)
        m._connected = False
        m._client = AsyncMock()
        await m.remove_device_config("t1", DeviceType.ONOFF)
        m._client.publish.assert_not_called()

    def test_entity_map_covers_all_types(self):
        mapped = set(DEVICE_ENTITY_MAP.keys())
        all_types = set(DeviceType) - {DeviceType.UNKNOWN}
        unmapped = all_types - mapped
        assert unmapped == set(), f"DeviceTypes without entity mapping: {unmapped}"

    def test_entity_map_each_type_has_valid_platform(self):
        valid_platforms = {"sensor", "binary_sensor", "switch", "light"}
        for dev_type, entities in DEVICE_ENTITY_MAP.items():
            for entity_name, platform, unit, device_class, icon in entities:
                assert platform in valid_platforms, f"{dev_type}: {entity_name} has invalid platform '{platform}'"

    def test_entity_map_no_empty_entries(self):
        for dev_type, entities in DEVICE_ENTITY_MAP.items():
            assert len(entities) > 0, f"{dev_type} has no entity mappings"

    def test_publish_state_disconnected_after_connect(self):
        m = MQTTDiscovery(host="test", port=1883)
        m._connected = True
        m._client = AsyncMock()
        # simulate connection drop
        m._connected = False
        # should not raise
        import asyncio
        asyncio.run(m.publish_state(BridgedDevice(id="t1", name="T", type=DeviceType.ONOFF, state={"power": True})))

    def test_publish_config_disconnected_after_connect(self):
        m = MQTTDiscovery(host="test", port=1883)
        m._connected = True
        m._client = AsyncMock()
        m._connected = False
        import asyncio
        asyncio.run(m.publish_device_config(BridgedDevice(id="t1", name="T", type=DeviceType.ONOFF)))
