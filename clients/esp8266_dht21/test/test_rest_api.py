#!/usr/bin/env python3
"""
Testes da API REST local do ESP8266 DHT11.

Endpoints:
  GET  /            -> pagina HTML
  GET  /api/state   -> JSON com temperatura/umidade

Uso:
  python3 test_rest_api.py --host <IP_do_ESP>
"""

import argparse
import json
import socket
import sys
import urllib.request
import urllib.error
from typing import Tuple

PASS = 0
FAIL = 1
_total = 0
_pass = 0

GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
BOLD = "\033[1m"
RESET = "\033[0m"


def test(name: str, cond: bool, detail: str = "") -> None:
    global _total, _pass
    _total += 1
    if cond:
        _pass += 1
        print(f"  {GREEN}[PASS]{RESET} {name}")
    else:
        print(f"  {RED}[FAIL]{RESET} {name}" + (f"  {RED}{detail}{RESET}" if detail else ""))


def request(host: str, method: str, path: str) -> Tuple[int, str, str]:
    url = f"http://{host}{path}"
    req = urllib.request.Request(url, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            body = resp.read().decode()
            return resp.status, body, resp.headers.get("Content-Type", "")
    except urllib.error.HTTPError as e:
        body = e.read().decode() if e.fp else ""
        return e.code, body, e.headers.get("Content-Type", "") if e.headers else ""
    except Exception as e:
        return -1, str(e), ""


def run_tests(host: str, verbose: bool) -> None:
    print(f"\n{'='*60}")
    print(f"  ESP8266 DHT11 - Testes da API REST")
    print(f"  Alvo: http://{host}")
    print(f"{'='*60}")

    print("\n[1] GET /  -> pagina HTML")
    status, body, ct = request(host, "GET", "/")
    test("status 200", status == 200, f"status={status}")
    test("content-type text/html", "text/html" in ct, f"ct={ct}")
    test("contem 'Temperatura'", "Temperatura" in body)
    test("contem 'Umidade'", "Umidade" in body)
    if verbose and status == 200:
        print(f"  --- HTML snippet ---\n{body[:400]}\n  -------------------")

    print("\n[2] GET /api/state  -> JSON")
    status, body, ct = request(host, "GET", "/api/state")
    test("status 200", status == 200, f"status={status}")
    test("content-type application/json", "application/json" in ct, f"ct={ct}")
    if status == 200:
        try:
            data = json.loads(body)
            test("JSON valido", True)
            test("contem 'temperature'", "temperature" in data)
            test("contem 'humidity'", "humidity" in data)
            test("contem 'device_id'", "device_id" in data)
            test("contem 'ip'", "ip" in data)
            test("'ip' nao vazio", bool(data.get("ip")))
            if verbose:
                print(f"  --- JSON ---\n{json.dumps(data, indent=2)}\n  -----------")
        except json.JSONDecodeError:
            test("JSON valido", False, f"body={body[:80]}")

    ok = _pass == _total
    print(f"\n{'='*60}")
    if ok:
        print(f"  {GREEN}{BOLD}[SUCESSO]{RESET} {_pass}/{_total} testes passaram")
    else:
        print(f"  {RED}{BOLD}[FALHA]{RESET} {_pass}/{_total} testes passaram")
    print(f"{'='*60}\n")


def check_network(host: str) -> None:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(2)
        s.connect((host, 80))
        local_ip = s.getsockname()[0]
        s.close()
        local_net = ".".join(local_ip.split(".")[:3])
        target_net = ".".join(host.split(".")[:3])
        if local_net != target_net:
            print(f"\n  {YELLOW}[ALERTA]{RESET} Rede incompativel!")
            print(f"  Local: {local_ip}  |  Alvo: {host}")
            print(f"  Dispositivo parece estar em rede diferente.\n")
            sys.exit(FAIL)
    except Exception:
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description="Testa a API REST do ESP8266 DHT11")
    parser.add_argument("--host", required=True, help="Endereco IP do ESP8266")
    parser.add_argument("--verbose", "-v", action="store_true", help="Exibir detalhes")
    args = parser.parse_args()
    check_network(args.host)
    run_tests(args.host, args.verbose)
    return PASS if _pass == _total else FAIL


if __name__ == "__main__":
    sys.exit(main())
