---
name: esp32-bridge
description: >-
  ESP32 Bridge + ESP8266 Clients — Use when building, flashing, or debugging the
  bridge firmware, UDP discovery, RainMaker gateway, or device registration.
---

# ESP32 Bridge + ESP8266 Clients

## Project Structure
- `main/` — ESP32 bridge firmware (IDF v5.5.4, RainMaker)
- `clients/esp8266_on_off/` — ESP8266 on/off client (Arduino)
- `clients/esp8266_dht11/` — ESP8266 temperature/humidity client
- `clients/esp8266_tanque/` — ESP8266 water tank level client

## Build & Flash
- `source config.sh && idf.py build` — compila bridge ESP32
- `./flash.sh` — source config + build + flash para `/dev/ttyUSB0`
- `./monitor.sh` — monitor serial (saída: `Ctrl+]`)
- `./erase.sh` — apaga flash
- Clientes ESP8266: compilar com Arduino IDE ou PlatformIO

## Key Conventions

### Device Identity
- `DEVICE_ID` removido — gerado dinamicamente como `esp8266_<chip_id>` via `ESP.getChipId()`
- `DEVICE_NAME` configurável via WiFiManager portal, persistido em EEPROM
- Validação: `is_valid_name()` rejeita bytes < 32 ou > 126 (evita 0xFF de EEPROM virgem)

### UDP Discovery
- Service name: `"esp-bridge"` (idêntico no cliente e bridge)
- Porta: 5000
- Cliente envia `{"service":"esp-bridge","discover":true,"id":"<device_id>"}` via broadcast
- Bridge responde com `{"service":"esp-bridge","ip_sta":"<ip>","http_port":80}`
- `BRIDGE_HOST "0.0.0.0"` no config.h força discovery; sem fallback fixo

### Bridge (ESP32)
- Registro HTTP: `POST /api/device/register`
- Estado HTTP: `POST /api/device/state`
- Heartbeat HTTP: `POST /api/device/heartbeat` (sem MQTT)
- Comandos HTTP: `GET /api/device/commands?id=<id>`
- RainMaker: `rmaker_gateway_device_add(id, type)` — lê nome do registry (`dev->name`)
- NVS: persistência de devices bridgeados via `nvs_set_blob`/`nvs_get_blob`
- cJSON: sempre copiar `valuestring` para buffer local com `strncpy` antes de `cJSON_Delete`

### Client (ESP8266)
- Retry de registro no `loop()`, não apenas no `setup()`
- `s_bridge_connected` indica status do bridge na API `/api/state`
- EEPROM para nome do dispositivo (marker `0xFF` + string + null terminator)

### RainMaker
- Sensores: usar `PROP_FLAG_READ` para params de só leitura
- Temperatura+Umidade: criar param `"Humidity"` manualmente
- Heartbeat separado de dados para economizar orçamento MQTT
- `CONFIG_LWIP_MAX_SOCKETS` deve ser aumentado se aparecer `ENFILE`

## Scripts
- `config.sh` — ambiente IDF (`$HOME/.espressif/v5.5.4/esp-idf`) + `RMAKER_PATH` (`$HOME/esp/esp-rainmaker`)
- `build.sh` — `idf.py build` (usa ccache)
- `flash.sh` — build + flash
- `monitor.sh` — monitor serial
- `erase.sh` — erase-flash

## Branches
- `main` — versão estável
- `dev` — desenvolvimento
- `main-v0.0.3` — backup do main anterior

## Style Guide
Ver `.opencode/skills/style-guide/SKILL.md` — design system do dashboard bridge (CSS variables, cards, badges, botões) e páginas inline dos clients ESP8266 (on/off, sensor).
