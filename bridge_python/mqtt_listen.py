#!/usr/bin/env python3
"""
Escuta topicos MQTT por um tempo determinado.

Uso:
  python mqtt_listen.py --seconds 30                    # escuta 30s, topico padrao
  python mqtt_listen.py --topic homeassistant/+/+/+/config --seconds 10
  python mqtt_listen.py --host localhost --port 1883
"""
from __future__ import annotations
import argparse
import asyncio
import os
import sys
import time

# auto-detect venv
_script_dir = os.path.dirname(os.path.abspath(__file__))
_venv_python = os.path.join(_script_dir, "venv", "bin", "python")
if os.path.exists(_venv_python) and sys.executable != _venv_python:
    os.execv(_venv_python, [_venv_python, __file__, *sys.argv[1:]])


async def run(host: str, port: int, topic: str, seconds: int):
    import aiomqtt

    print(f"Conectando ao MQTT {host}:{port}...")
    print(f"Topico: {topic}")
    print(f"Tempo: {seconds}s")
    print("-" * 50)

    try:
        async with aiomqtt.Client(hostname=host, port=port) as client:
            await client.subscribe(topic)
            start = time.time()
            count = 0
            async with asyncio.timeout(seconds):
                async for msg in client.messages:
                    elapsed = time.time() - start
                    payload = msg.payload.decode(errors="replace")
                    print(f"[{elapsed:6.1f}s] {msg.topic}: {payload}")
                    count += 1
    except asyncio.TimeoutError:
        pass

    print("-" * 50)
    print(f"Total: {count} mensagens em {seconds}s")


def main():
    parser = argparse.ArgumentParser(description="Escuta topicos MQTT")
    parser.add_argument("--host", default="localhost", help="Host MQTT")
    parser.add_argument("--port", type=int, default=1883, help="Porta MQTT")
    parser.add_argument("--topic", default="homeassistant/#", help="Topico ou pattern")
    parser.add_argument("-s", "--seconds", type=int, default=10, help="Tempo de escuta em segundos")
    args = parser.parse_args()

    try:
        asyncio.run(run(args.host, args.port, args.topic, args.seconds))
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuario")


if __name__ == "__main__":
    main()
