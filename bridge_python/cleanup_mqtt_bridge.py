#!/usr/bin/env python3
"""
Limpa todos os registros MQTT Discovery do bridge ESP32 no Home Assistant.
Remove entidades de devices que não estão mais registrados no bridge_python.

Uso:
  python cleanup_mqtt_bridge.py --dry-run              # mostra o que seria removido
  python cleanup_mqtt_bridge.py --host 192.168.1.100   # MQTT em outro host
"""
from __future__ import annotations
import os
import sys

# auto-detect venv relativa ao script
_script_dir = os.path.dirname(os.path.abspath(__file__))
_venv_python = os.path.join(_script_dir, "venv", "bin", "python")
if os.path.exists(_venv_python) and sys.executable != _venv_python:
    os.execv(_venv_python, [_venv_python, __file__, *sys.argv[1:]])

import argparse
import asyncio
import json

DISCOVERY_PREFIX = "homeassistant"
VIA_DEVICE = "esp32_bridge"


async def run(host: str, port: int, dry_run: bool):
    import aiomqtt

    print(f"Conectando ao MQTT {host}:{port}...")
    try:
        async with aiomqtt.Client(hostname=host, port=port) as client:
            print("Conectado. Coletando topicos de discovery...")
            topics: dict[str, dict] = {}

            messages = client.messages
            await client.subscribe(f"{DISCOVERY_PREFIX}/+/+/+/config")
            try:
                async with asyncio.timeout(60):
                    async for msg in messages:
                        try:
                            payload = json.loads(msg.payload.decode())
                        except (json.JSONDecodeError, UnicodeDecodeError):
                            continue
                        via = payload.get("device", {}).get("via_device", "")
                        if via == VIA_DEVICE:
                            topics[msg.topic.value] = payload
            except asyncio.TimeoutError:
                pass

            if not topics:
                print("Nenhum registro do bridge ESP32 encontrado.")
                return

            print(f"\nEncontrados {len(topics)} topicos do bridge ESP32:")
            for topic in sorted(topics):
                name = topics[topic].get("name", "?")
                print(f"  {topic}  ({name})")

            if dry_run:
                print(f"\nDry-run: {len(topics)} topicos seriam limpos.")
                return

            print(f"\nLimpando {len(topics)} topicos...")
            for topic in topics:
                await client.publish(topic, payload="", retain=True)
                print(f"  limpo: {topic}")

            print("\nLimpeza concluida. Recarregue o MQTT no HA ou reinicie o bridge.")
    except Exception as e:
        print(f"ERRO: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Limpa MQTT Discovery do bridge ESP32", add_help=False)
    parser.add_argument("command", nargs="?", choices=["help"], help="Subcomando (help)")
    parser.add_argument("--host", default="localhost", help="Host MQTT")
    parser.add_argument("--port", type=int, default=1883, help="Porta MQTT")
    parser.add_argument("--dry-run", action="store_true", help="Apenas mostra o que seria removido")
    parser.add_argument("-h", "--help", action="store_true", dest="show_help")
    args = parser.parse_args()

    if args.show_help or args.command == "help":
        parser.print_help()
        return

    asyncio.run(run(args.host, args.port, args.dry_run))


if __name__ == "__main__":
    main()
