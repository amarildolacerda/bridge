#!/usr/bin/env python3
"""
Integration tests for ESP32 Bridge HTTP REST API.

Usage:
    python test/test_integration.py --host 192.168.1.202 [--verbose]
    python test/test_integration.py --host espbridge.local [--skip-reset]

Options:
    --host        Bridge IP or hostname (default: espbridge.local)
    --port        HTTP port (default: 80)
    --verbose     Print detailed responses
    --skip-reset  Skip tests that reset/modify bridge state
"""

import argparse
import json
import sys
import time
import urllib.request
import urllib.error

PASS = 0
FAIL = 0
SKIP = 0
VERBOSE = False


def log(msg=""):
    print(f"  {msg}")


def log_verbose(msg):
    if VERBOSE:
        print(f"    DEBUG: {msg}")


def request(method, url, data=None, timeout=5):
    """Send HTTP request and return (status, body)."""
    if data is not None:
        body = json.dumps(data).encode()
        req = urllib.request.Request(url, data=body, method=method)
        req.add_header("Content-Type", "application/json")
    else:
        req = urllib.request.Request(url, method=method)

    try:
        resp = urllib.request.urlopen(req, timeout=timeout)
        status = resp.status
        body = resp.read().decode()
        return status, body
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()
    except urllib.error.URLError as e:
        return 0, str(e)
    except Exception as e:
        return 0, str(e)


def test(name, func):
    """Run a test case."""
    global PASS, FAIL
    label = f"  TEST: {name:55s} "
    print(label, end="", flush=True)
    try:
        func()
        print("PASSED")
        PASS += 1
    except AssertionError as e:
        print(f"FAILED\n    {e}")
        FAIL += 1
    except Exception as e:
        print(f"FAILED (exception: {e})")
        FAIL += 1


def assert_eq(expected, actual, msg=""):
    if expected != actual:
        raise AssertionError(f"expected {expected!r}, got {actual!r}" + (f" - {msg}" if msg else ""))


def assert_in(haystack, needle, msg=""):
    if needle not in haystack:
        raise AssertionError(f"expected {needle!r} in {haystack!r}" + (f" - {msg}" if msg else ""))


def assert_not_in(haystack, needle, msg=""):
    if needle in haystack:
        raise AssertionError(f"unexpected {needle!r} in {haystack!r}" + (f" - {msg}" if msg else ""))


def assert_key(key, obj, msg=""):
    if key not in obj:
        raise AssertionError(f"missing key {key!r}" + (f" - {msg}" if msg else ""))


def assert_type(expected_type, value, msg=""):
    if not isinstance(value, expected_type):
        raise AssertionError(f"expected {expected_type.__name__}, got {type(value).__name__}" + (f" - {msg}" if msg else ""))


# ---------------------------------------------------------------------------
# Test suite
# ---------------------------------------------------------------------------

def make_suite(base_url, skip_reset):
    """Generate test functions for a given bridge URL."""

    ts = int(time.time())
    _device_id = f"test_dev1_{ts}"
    _device_id2 = f"test_dev2_{ts}"

    def test_ping():
        """GET /api/ping - health check"""
        status, body = request("GET", f"{base_url}/api/ping")
        assert_eq(200, status)
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_dashboard():
        """GET / - dashboard HTML"""
        status, body = request("GET", base_url)
        assert_eq(200, status)
        assert_in(body, "ESP32 RainMaker Gateway")

    def test_dashboard_css():
        """GET /dashboard.css - CSS"""
        status, body = request("GET", f"{base_url}/dashboard.css")
        assert_eq(200, status)
        assert_in(body, ":root")

    def test_gateway_info():
        """GET /api/gateway/info - bridge status"""
        status, body = request("GET", f"{base_url}/api/gateway/info")
        assert_eq(200, status)
        data = json.loads(body)
        assert_key("ip", data)
        assert_key("uptime_s", data)
        assert_type(str, data["ip"])

    def test_devices_empty():
        """GET /api/devices - empty list"""
        status, body = request("GET", f"{base_url}/api/devices")
        assert_eq(200, status)
        data = json.loads(body)
        assert_key("devices", data)
        assert_type(list, data["devices"])

    def test_register_device_minimal():
        """POST /api/device/register - minimal (id + type)"""
        status, body = request("POST", f"{base_url}/api/device/register",
                               {"id": _device_id, "type": "onoff"})
        assert_eq(200, status, f"body={body}")
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_register_device_full():
        """POST /api/device/register - full (id + type + name + ip)"""
        status, body = request("POST", f"{base_url}/api/device/register",
                               {"id": _device_id2, "type": "temperature",
                                "name": "Temp Sensor", "ip": "10.0.0.5"})
        assert_eq(200, status, f"body={body}")
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_register_duplicate():
        """POST /api/device/register - duplicate (refresh)"""
        status, body = request("POST", f"{base_url}/api/device/register",
                               {"id": _device_id, "type": "dimmable", "name": "Updated"})
        assert_eq(200, status, f"body={body}")
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_register_missing_id():
        """POST /api/device/register - missing id"""
        status, body = request("POST", f"{base_url}/api/device/register",
                               {"type": "onoff"})
        assert_eq(400, status)

    def test_register_missing_type():
        """POST /api/device/register - missing type"""
        status, body = request("POST", f"{base_url}/api/device/register",
                               {"id": "no_type_dev"})
        assert_eq(400, status)

    def test_register_invalid_type():
        """POST /api/device/register - invalid type"""
        status, body = request("POST", f"{base_url}/api/device/register",
                               {"id": "bad_type", "type": "invalid_type"})
        assert_eq(400, status)

    def test_register_invalid_json():
        """POST /api/device/register - invalid JSON"""
        req = urllib.request.Request(
            f"{base_url}/api/device/register",
            data=b"not json",
            method="POST")
        req.add_header("Content-Type", "application/json")
        try:
            resp = urllib.request.urlopen(req, timeout=5)
            assert_eq(400, resp.status)
        except urllib.error.HTTPError as e:
            assert_eq(400, e.code)

    def test_devices_after_register():
        """GET /api/devices - after registering"""
        status, body = request("GET", f"{base_url}/api/devices")
        assert_eq(200, status)
        data = json.loads(body)
        devices = data.get("devices", [])
        ids = [d.get("id") for d in devices]
        assert_in(ids, _device_id)

    def test_device_info():
        """GET /api/device/info?id=X"""
        status, body = request("GET", f"{base_url}/api/device/info?id={_device_id}")
        assert_eq(200, status)
        data = json.loads(body)
        assert_eq(_device_id, data.get("id"))
        assert_key("type", data)
        assert_key("online", data)

    def test_device_info_missing_id():
        """GET /api/device/info - missing id"""
        status, body = request("GET", f"{base_url}/api/device/info")
        assert_eq(400, status)

    def test_device_info_not_found():
        """GET /api/device/info - nonexistent"""
        status, body = request("GET", f"{base_url}/api/device/info?id=nonexistent")
        assert_eq(404, status)

    def test_update_state():
        """POST /api/device/state - update"""
        status, body = request("POST", f"{base_url}/api/device/state",
                               {"id": _device_id, "power": "on"})
        assert_eq(200, status, f"body={body}")
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_update_state_not_found():
        """POST /api/device/state - nonexistent"""
        status, body = request("POST", f"{base_url}/api/device/state",
                               {"id": "ghost", "power": "on"})
        assert_eq(404, status)

    def test_update_state_multiple():
        """POST /api/device/state - multiple keys"""
        status, body = request("POST", f"{base_url}/api/device/state",
                               {"id": _device_id, "brightness": "75", "color": "red"})
        assert_eq(200, status)

    def test_device_info_shows_state():
        """GET /api/device/info - state included"""
        request("POST", f"{base_url}/api/device/state",
                {"id": _device_id, "temp": "25.5"})
        status, body = request("GET", f"{base_url}/api/device/info?id={_device_id}")
        assert_eq(200, status)
        data = json.loads(body)
        assert_key("type", data)

    def test_commands_empty():
        """GET /api/device/commands?id=X - empty"""
        status, body = request("GET", f"{base_url}/api/device/commands?id={_device_id}")
        assert_eq(200, status)
        data = json.loads(body)
        assert_key("commands", data)
        assert_eq([], data["commands"])

    def test_commands_post_empty():
        """POST /api/device/commands - empty via body"""
        status, body = request("POST", f"{base_url}/api/device/commands",
                               {"id": _device_id})
        assert_eq(200, status)
        data = json.loads(body)
        assert_eq([], data["commands"])

    def test_commands_missing_id():
        """GET /api/device/commands - missing id"""
        status, body = request("GET", f"{base_url}/api/device/commands")
        assert_eq(400, status)

    def test_heartbeat():
        """POST /api/device/heartbeat"""
        status, body = request("POST", f"{base_url}/api/device/heartbeat",
                               {"id": _device_id})
        assert_eq(200, status)
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_heartbeat_missing_id():
        """POST /api/device/heartbeat - missing id"""
        status, body = request("POST", f"{base_url}/api/device/heartbeat", {"x": "y"})
        assert_eq(400, status)

    def test_qrcode():
        """GET /api/qrcode"""
        status, body = request("GET", f"{base_url}/api/qrcode")
        assert_eq(200, status)
        data = json.loads(body)
        assert_key("service_name", data)
        assert_key("transport", data)
        assert_key("qr", data)

    def test_remove_device():
        """POST /api/device/remove"""
        status, body = request("POST", f"{base_url}/api/device/remove",
                               {"id": _device_id2})
        assert_eq(200, status)
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    def test_remove_missing_id():
        """POST /api/device/remove - missing id"""
        status, body = request("POST", f"{base_url}/api/device/remove", {"x": "y"})
        assert_eq(400, status)

    def test_remove_not_found():
        """POST /api/device/remove - nonexistent"""
        status, body = request("POST", f"{base_url}/api/device/remove",
                               {"id": "already_gone"})
        assert_eq(404, status)

    def test_broadcast():
        """POST /api/gateway/broadcast"""
        status, body = request("POST", f"{base_url}/api/gateway/broadcast")
        assert_eq(200, status)
        data = json.loads(body)
        assert_eq("ok", data.get("status"))

    # -- Build ordered test list --
    tests = [
        ("ping", test_ping, False),
        ("dashboard", test_dashboard, False),
        ("dashboard_css", test_dashboard_css, False),
        ("gateway_info", test_gateway_info, False),
        ("devices_empty", test_devices_empty, False),
        ("register_minimal", test_register_device_minimal, skip_reset),
        ("register_full", test_register_device_full, skip_reset),
        ("register_duplicate", test_register_duplicate, skip_reset),
        ("register_missing_id", test_register_missing_id, False),
        ("register_missing_type", test_register_missing_type, False),
        ("register_invalid_type", test_register_invalid_type, False),
        ("register_invalid_json", test_register_invalid_json, False),
        ("devices_after_register", test_devices_after_register, skip_reset),
        ("device_info", test_device_info, skip_reset),
        ("device_info_missing_id", test_device_info_missing_id, False),
        ("device_info_not_found", test_device_info_not_found, False),
        ("update_state", test_update_state, skip_reset),
        ("update_state_not_found", test_update_state_not_found, False),
        ("update_state_multiple", test_update_state_multiple, skip_reset),
        ("device_info_shows_state", test_device_info_shows_state, skip_reset),
        ("commands_empty", test_commands_empty, skip_reset),
        ("commands_post_empty", test_commands_post_empty, skip_reset),
        ("commands_missing_id", test_commands_missing_id, False),
        ("heartbeat", test_heartbeat, skip_reset),
        ("heartbeat_missing_id", test_heartbeat_missing_id, False),
        ("qrcode", test_qrcode, False),
        ("remove_device", test_remove_device, skip_reset),
        ("remove_missing_id", test_remove_missing_id, False),
        ("remove_not_found", test_remove_not_found, False),
        ("broadcast", test_broadcast, skip_reset),
    ]

    return tests


def main():
    global VERBOSE
    parser = argparse.ArgumentParser(description="Integration tests for ESP32 Bridge")
    parser.add_argument("--host", default="espbridge.local", help="Bridge hostname/IP")
    parser.add_argument("--port", type=int, default=80, help="HTTP port")
    parser.add_argument("--verbose", action="store_true", help="Show detailed responses")
    parser.add_argument("--skip-reset", action="store_true",
                        help="Skip tests that modify bridge state (register/remove)")
    args = parser.parse_args()
    VERBOSE = args.verbose

    base_url = f"http://{args.host}:{args.port}"

    print(f"ESP32 Bridge Integration Tests")
    print(f"Target: {base_url}")
    print(f"Skip modify tests: {args.skip_reset}")
    print("=" * 70)

    # Check connectivity first
    status, body = request("GET", f"{base_url}/api/ping", timeout=3)
    if status != 200:
        print(f"  Bridge not reachable at {base_url}/api/ping")
        print(f"  Response: status={status}, body={body[:200]}")
        print(f"\n  Make sure the bridge is powered on and connected to the network.")
        sys.exit(1)

    print(f"  Bridge reachable (ping OK)")
    print()

    tests = make_suite(base_url, args.skip_reset)

    for name, func, should_skip in tests:
        if should_skip:
            global SKIP
            SKIP += 1
            print(f"  TEST: {name:55s} SKIPPED")
            continue
        test(name, func)

    total = PASS + FAIL
    print()
    print("=" * 70)
    print(f"  Results: {PASS}/{total} passed, {FAIL} failed, {SKIP} skipped")
    print("=" * 70)

    return 1 if FAIL > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
