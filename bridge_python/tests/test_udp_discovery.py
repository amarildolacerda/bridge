from __future__ import annotations
import json
import pytest
from app.udp_discovery import UDPDiscovery, DISCOVERY_SERVICE


class TestUDPDiscovery:
    @pytest.fixture
    def discovery(self):
        ud = UDPDiscovery(bridge_ip="192.168.1.50", http_port=80)
        yield ud

    def test_handle_discover_request(self, discovery):
        data = json.dumps({
            "service": DISCOVERY_SERVICE,
            "discover": True,
            "id": "esp8266_test",
        }).encode()
        discovery._handle_message(data, ("10.0.0.1", 5000))
        assert "esp8266_test" in discovery._discovered_ips
        assert discovery._discovered_ips["esp8266_test"][0] == "10.0.0.1"

    def test_handle_wrong_service(self, discovery):
        data = json.dumps({"service": "wrong"}).encode()
        discovery._handle_message(data, ("10.0.0.1", 5000))
        assert len(discovery._discovered_ips) == 0

    def test_prune_discovered(self, discovery):
        import time
        discovery._discovered_ips["old"] = ("10.0.0.1", time.time() - 600)
        discovery._discovered_ips["new"] = ("10.0.0.2", time.time())
        discovery._prune_discovered()
        assert "old" not in discovery._discovered_ips
        assert "new" in discovery._discovered_ips

    def test_do_broadcast_returns_dict(self, discovery):
        result = discovery.do_broadcast()
        assert "registered" in result
        assert "discovered" in result
