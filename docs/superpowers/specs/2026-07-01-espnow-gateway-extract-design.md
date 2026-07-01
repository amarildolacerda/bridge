# ESP-NOW Gateway — Extração para repo próprio

## Objetivo

Criar um repositório git independente `espnow_gateway` contendo:
- Firmware ESP8266 gateway (ESP-NOW → HTTP)
- Bridge Python como submodule

## Motivação

O gateway ESP-NOW (`clients/esp8266_gateway`) é um projeto autônomo que conversa com o bridge via HTTP/UDP. Separar do repositório principal do bridge permite:
- CI/CD independente
- Versionamento próprio
- Isolamento de responsabilidades

## Estrutura

```
espnow_gateway/
├── gateway/               # firmware ESP8266 (PlatformIO)
│   ├── include/
│   │   ├── bridge_client.h
│   │   ├── config.h
│   │   ├── espnow_handler.h
│   │   ├── espnow_protocol.h
│   │   ├── ota.h
│   │   ├── pages.h
│   │   ├── sensor_registry.h
│   │   └── web_server.h
│   ├── src/
│   │   ├── bridge_client.cpp
│   │   ├── espnow_handler.cpp
│   │   ├── main.cpp
│   │   ├── ota.cpp
│   │   ├── sensor_registry.cpp
│   │   └── web_server.cpp
│   ├── platformio.ini
│   ├── build.sh
│   └── monitor.sh
├── bridge_python/         # submodule → github.com/amarildolacerda/bridge_python
├── README.md
└── .gitignore
```

## Arquitetura

```
[Sensores ESP-NOW] ←→ [Gateway ESP8266] ←HTTP→ [Bridge Python]
                                                    ↓
                                               [MQTT / HA]
```

1. **Gateway ESP8266** (`gateway/`): recebe dados de sensores via ESP-NOW, mantém registro de sensores pareados, expõe dashboard web, comunica-se com o bridge via HTTP REST
2. **Bridge Python** (`bridge_python/` submodule): servidor HTTP com UDP discovery, registry de devices, MQTT discovery para Home Assistant
3. **Comunicação**: gateway registra cada sensor ESP-NOW como device virtual no bridge via `POST /api/device/register`, envia dados via `POST /api/device/state`, heartbeat via `POST /api/device/heartbeat`

## Submodule

- URL: `https://github.com/amarildolacerda/bridge_python.git`
- Path: `bridge_python/`
- Branch: `main`

## Passos de implementação

1. Criar diretório `/home/kzuca/project/espnow_gateway/`
2. `git init`
3. Copiar `clients/esp8266_gateway/` para `gateway/`
4. Ajustar `platformio.ini` se necessário (paths relativos)
5. `git submodule add https://github.com/amarildolacerda/bridge_python.git`
6. Criar `README.md` e `.gitignore`
7. Commit inicial
8. Verificar build do firmware
