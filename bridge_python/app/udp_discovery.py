from __future__ import annotations
import asyncio
import json
import logging
import socket
import time

LOG = logging.getLogger(__name__)

DISCOVERY_PORT = 5000
DISCOVERY_SERVICE = "esp-bridge"
BROADCAST_INTERVAL = 300
BROADCAST_DELAY = 3
MAX_DISCOVERED_IPS = 8
DISCOVERED_IP_TIMEOUT = 300


class _UDPProtocol(asyncio.DatagramProtocol):
    def __init__(self, discovery: UDPDiscovery):
        self._discovery = discovery

    def datagram_received(self, data: bytes, addr: tuple[str, int]):
        self._discovery._handle_message(data, addr)


def _get_local_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(1)
        s.connect(("192.168.1.1", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "0.0.0.0"


class UDPDiscovery:
    def __init__(self, bridge_ip: str = "", http_port: int = 80):
        self._bridge_ip = bridge_ip or _get_local_ip()
        self._http_port = http_port
        self._discovered_ips: dict[str, tuple[str, float]] = {}
        self._running = False
        self._start_time = 0.0
        self._sock: socket.socket | None = None
        self._transport: asyncio.DatagramTransport | None = None

    async def start(self):
        self._running = True
        self._start_time = time.time()
        loop = asyncio.get_event_loop()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setblocking(False)
        sock.bind(("0.0.0.0", DISCOVERY_PORT))
        self._sock = sock
        transport, _ = await loop.create_datagram_endpoint(
            lambda: _UDPProtocol(self),
            sock=sock,
        )
        self._transport = transport
        LOG.info("UDP discovery listening on port %d", DISCOVERY_PORT)
        self._broadcast_task = asyncio.create_task(self._broadcast_loop())

    async def stop(self):
        self._running = False
        self._broadcast_task.cancel()
        if self._transport:
            self._transport.close()
        if self._sock:
            self._sock.close()

    def _handle_message(self, data: bytes, addr: tuple[str, int]):
        try:
            msg = json.loads(data.decode())
        except (json.JSONDecodeError, UnicodeDecodeError):
            return
        service = msg.get("service")
        if service != DISCOVERY_SERVICE:
            return
        if msg.get("discover") is True:
            sender_id = msg.get("id", addr[0])
            self._discovered_ips[sender_id] = (addr[0], time.time())
            self._prune_discovered()
            self._send_response(addr[0])

    def _send_response(self, target_ip: str):
        msg = json.dumps({
            "service": DISCOVERY_SERVICE,
            "ip_sta": self._bridge_ip,
            "http_port": self._http_port,
        }).encode()
        try:
            if self._sock:
                self._sock.sendto(msg, (target_ip, DISCOVERY_PORT))
        except Exception:
            LOG.exception("UDP send error")

    async def _broadcast_loop(self):
        await asyncio.sleep(BROADCAST_DELAY)
        while self._running:
            self._send_broadcast()
            await asyncio.sleep(BROADCAST_INTERVAL)

    def _send_broadcast(self):
        uptime = int(time.time() - self._start_time)
        msg = json.dumps({
            "service": DISCOVERY_SERVICE,
            "ip_sta": self._bridge_ip,
            "http_port": self._http_port,
            "uptime_s": uptime,
            "re_register": True,
        }).encode()
        try:
            if self._sock:
                self._sock.sendto(msg, ("255.255.255.255", DISCOVERY_PORT))
        except Exception:
            LOG.exception("UDP broadcast error")

    def do_broadcast(self) -> dict:
        self._send_broadcast()
        return {
            "registered": [],
            "discovered": [
                {"id": did, "ip": ip}
                for did, (ip, _) in self._discovered_ips.items()
            ],
        }

    def get_bridge_ip(self) -> str:
        return self._bridge_ip

    def _prune_discovered(self):
        now = time.time()
        to_remove = [
            did for did, (_, ts) in self._discovered_ips.items()
            if now - ts > DISCOVERED_IP_TIMEOUT
        ]
        for did in to_remove:
            del self._discovered_ips[did]
