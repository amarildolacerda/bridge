#!/usr/bin/env python3
"""
Discover bridge IP via UDP broadcast on port 5000.

Usage:
    python test/discover_bridge.py                    # auto-discover
    python test/discover_bridge.py --wait 15          # longer timeout
    python test/discover_bridge.py --json             # JSON output

Returns exit code 0 if bridge found, 1 if not found.
"""
import argparse
import json
import socket
import sys
import time

DISCOVERY_PORT = 5000
SERVICE_NAME = "esp-bridge"


def discover(timeout=10):
    """Send UDP discovery request and listen for bridge response.
    Returns list of dicts with bridge info."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("", DISCOVERY_PORT))
    sock.settimeout(0.5)

    payload = json.dumps({"service": SERVICE_NAME, "discover": True}).encode()
    results = []
    deadline = time.time() + timeout

    # Send discovery request
    sock.sendto(payload, ("255.255.255.255", DISCOVERY_PORT))

    while time.time() < deadline:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode("utf-8", errors="replace")
            parsed = json.loads(msg)
            if parsed.get("service") == SERVICE_NAME:
                ip = parsed.get("ip_sta", addr[0])
                port = parsed.get("http_port", 80)
                results.append({
                    "ip": ip,
                    "port": port,
                    "mac": addr[0],
                    "raw": msg,
                })
        except (socket.timeout, json.JSONDecodeError, UnicodeDecodeError):
            pass

    sock.close()
    return results


def main():
    parser = argparse.ArgumentParser(description="Discover ESP32 Bridge via UDP")
    parser.add_argument("--wait", type=int, default=10, help="Discovery timeout (s)")
    parser.add_argument("--json", action="store_true", help="JSON output")
    args = parser.parse_args()

    results = discover(timeout=args.wait)

    if not results:
        if args.json:
            print(json.dumps({"found": False, "bridges": []}))
        else:
            print(f"No bridge found via UDP broadcast (port {DISCOVERY_PORT})")
        return 1

    if args.json:
        print(json.dumps({"found": True, "bridges": results}))
    else:
        for r in results:
            print(f"Bridge: {r['ip']}:{r['port']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
