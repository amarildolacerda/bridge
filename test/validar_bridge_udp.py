#!/usr/bin/env python3
"""
Valida se o bridge ESP32 está respondendo a descoberta UDP.
Escuta broadcasts na porta 5000 e envia discovery requests.
"""
import socket
import json
import sys
import time
import threading
from datetime import datetime

DISCOVERY_PORT = 5000
TIMEOUT = 10

def listen_broadcast(stop_event, results):
    """Escuta broadcasts e respostas na porta 5000"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(('', DISCOVERY_PORT))
    sock.settimeout(1)

    while not stop_event.is_set():
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace')
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]

            try:
                parsed = json.loads(msg)
                svc = parsed.get("service", "")
                ip = parsed.get("ip_sta", "")
                port = parsed.get("http_port", "")
                results.append({
                    "time": ts,
                    "from": addr,
                    "service": svc,
                    "ip": ip,
                    "port": port,
                    "raw": msg
                })
                print(f"[{ts}] BRIDGE RESPONSE from {addr[0]}:{addr[1]}")
                print(f"        service={svc} ip={ip} port={port}")
            except json.JSONDecodeError:
                print(f"[{ts}] Unknown packet from {addr[0]}: {msg[:100]}")

        except socket.timeout:
            continue
        except Exception as e:
            print(f"Error: {e}")

    sock.close()

def send_discovery(sock):
    """Envia discovery request via broadcast"""
    payload = json.dumps({"service": "esp-bridge", "discover": True})
    sock.sendto(payload.encode(), ('255.255.255.255', DISCOVERY_PORT))
    print(f"  -> Discovery sent to 255.255.255.255:{DISCOVERY_PORT}")

def main():
    print("=" * 60)
    print(" VALIDACAO DE BROADCAST UDP DO BRIDGE")
    print("=" * 60)

    stop = threading.Event()
    results = []

    listener = threading.Thread(target=listen_broadcast, args=(stop, results), daemon=True)
    listener.start()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(1)

    input("\nPressione ENTER para enviar discovery request...")
    send_discovery(sock)

    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        if any(r for r in results if r.get("ip") and r["ip"] != "0.0.0.0"):
            break
        time.sleep(0.5)

    sock.close()

    stop.set()
    listener.join(timeout=3)

    print("\n" + "=" * 60)
    print(" RESULTADOS")
    print("=" * 60)

    valid = [r for r in results if r.get("ip") and r["ip"] != "0.0.0.0"]

    if valid:
        for r in valid:
            print(f"  [OK] Bridge encontrado em {r['ip']}:{r['port']}")
            print(f"       Servico: {r['service']}")
            print(f"       Origem: {r['from'][0]}")
            print(f"       Resposta bruta: {r['raw']}")
        print(f"\n  Bridge respondendo corretamente!")
        return 0
    else:
        if results:
            print(f"  Pacotes recebidos: {len(results)}")
            for r in results:
                print(f"  - De {r['from'][0]}: {r['raw']}")
        else:
            print(f"  Nenhum pacote recebido na porta {DISCOVERY_PORT}")

        print(f"\n  Bridge NAO respondendo. Possiveis causas:")
        print(f"  1. Bridge nao esta na rede (sem WiFi)")
        print(f"  2. Bridge esta em IP diferente (ver monitor serial)")
        print(f"  3. Firewall bloqueando broadcast UDP na porta {DISCOVERY_PORT}")
        print(f"  4. AP isolation ativado no roteador")
        return 1

if __name__ == "__main__":
    sys.exit(main())
