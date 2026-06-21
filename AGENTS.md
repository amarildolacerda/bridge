# ESP32 Bridge + ESP8266 Clients — Projeto

## Branches
- `main` — estável, usado nos dispositivos em produção
- `dev` — desenvolvimento
- a atualizao do "dev" para "main" so pode ser feito se solicitado ou pegar autorização
- `main-v0.0.3` — backup do main anterior (antes do reset para dev)
- antes de passar o dev para main gerar um branch do main_vx.x.x
- quando gerar uma nova tag (ex: v0.0.7) tornar a versão a mesma da tag (target_compile_definitions(${COMPONENT_LIB} PRIVATE FW_VERSION="v0.0.7"))

## Ambiente

### Pré-requisitos
- `sudo apt install ccache git curl` — ccache acelera rebuilds
- Python 3.10+ com `venv`

### ESP-IDF (primeira instalação)
```sh
mkdir -p ~/.espressif/v5.5.4
git clone --recursive -b v5.5.4 https://github.com/espressif/esp-idf.git ~/.espressif/v5.5.4/esp-idf
```
> Se o clone falhar por falta de internet, reconecte a rede e execute:
> ```sh
> git -C ~/.espressif/v5.5.4/esp-idf submodule update --init --recursive
> ```

### RainMaker
```sh
git clone --recursive -b v1.8.2 https://github.com/espressif/esp-rainmaker.git ~/esp/esp-rainmaker
```

## Build
1. `source config.sh` carrega IDF v5.5.4 + RMAKER_PATH (ativa ccache automaticamente)
2. `idf.py build` compila bridge ESP32
3. Clientes ESP8266: compilar separadamente (Arduino IDE / PlatformIO)

## Scripts
- `build.sh` — só build
- `flash.sh [-p <port>]` — source + build + flash (porta padrão `/dev/ttyUSB0`)
- `monitor.sh` — source + monitor (saída: `Ctrl+]`)
- `monitor.py` — monitor serial Python, sai com `q` ou `Ctrl+C`
- `erase.sh` — source + erase-flash

## Arquitetura
- Bridge (ESP32/IDF): servidor HTTP REST + RainMaker + discovery UDP + terminal serial
- Clients (ESP8266/Arduino): sensores/atuadores que se registram no bridge via HTTP
- Discovery UDP: broadcast porta 5000, service name `"esp-bridge"`

## Terminal do Bridge (console serial)
- `l` — lista devices registrados (com índices numéricos)
- `s` — status do bridge (IP, total devices, uptime)
- `d <id|índice>` — detalhes de um device (aceita ID ou número da lista `l`)
- `b` — broadcast re-register (envia `re_register:true` via UDP, clients re-registram via HTTP, mostra registrados + descobertos)
- `r` — restart
- `h` / `?` — ajuda
- Usa `getchar()` single-key, prompt `bridge>` só aparece após comando executado
- para descobrir o ip do Bridge rodar discover_bridge.py

## Provisionamento WiFi
- Bridge usa **BLE** (não SoftAP): quando não há credenciais STA salvas, inicia BLE advertising `PROV_<nome>`
- Provisionamento feito via app ESP RainMaker (escaneia QR code ou conecta via BLE)
- POP_TYPE_NONE + Security v1 — sem PIN, app conecta direto
- QR code gerado com `app_network_get_device_service_name()` + `app_network_get_device_pop(POP_TYPE_RANDOM)` no dashboard em `/api/qrcode`
- Após provisionamento via BLE, bridge conecta ao WiFi e inicia RainMaker cloud
- `app_main.cpp` — init: network → event handlers → rmaker_gateway → esp_rmaker_start → app_network_start (BLE, POP_TYPE_NONE)

## Desenvolvimento
- Alterações de código devem ser feitas apenas no branch `dev`. Verifique com `git branch --show-current` antes de começar.
- `main` é estável e usado em produção — nunca commitar diretamente em `main`.
### Novos Clients
1. sempre ter um README.md para orientar as conexões de hardwares/pinos e demais informações releventes ao cliente
2. ter um dashboard pertinent 
3. se possivel ter configuração WIFI com WiFiManager
4. ter atalhos de teclados no terminal
5. seguir API do gateway

## Regras importantes
1. Device ID é dinâmico (`esp8266_<chip_id>`), não configurável
2. Device name configurável via WiFiManager, salvo em EEPROM com validação (> 32, < 127)
3. BRIDGE_HOST = "0.0.0.0" força discovery UDP (sem fallback fixo)
4. Sempre copiar cJSON `valuestring` para buffer local com `strncpy` antes de `cJSON_Delete`
5. Retry de registro no `loop()`, não só no `setup()`
6. `CONFIG_LWIP_MAX_SOCKETS` precisa ser aumentado se aparecer `ENFILE`
7. Persistir devices bridgeados em NVS para restaurar no boot
8. Clients enviam `bridge_connected` no `/api/state`
9. DHT21 client: GPIO 5, tipo DHT21, fallback `isnan()` não envia ao bridge (flag `s_dht_valid`)
10. Clients respondem a `re_register:true` no UDP registrando novamente via HTTP POST `/api/device/register`
11. Bridge broadcast (`b`): envia `re_register:true` via UDP, clients re-registram via HTTP, mostra registrados + descobertos
12. Dashboard web tem card QR code do RainMaker em `/api/qrcode`

## Regras de AI
0. economizar tokens com respostas minimas sem explicações desnecessaria 
1. manter skills enxutas
2. economizar tokens simplificando com a comunição
3. manter uma comunicação objetiva sem rodeios

### Clientes estáveis (não modificar)
- `clients/esp8266_gas/` — estável, não aplicar alterações
- `clients/esp8266_dht21/` — estável, não aplicar alterações
- `clients/esp8266_chuva/` — estável, não aplicar alterações
