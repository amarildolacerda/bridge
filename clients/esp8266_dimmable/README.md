# ESP8266 Dimmable Light

Cliente ESP8266 para controle de lâmpada dimerizável via PWM. Recebe comandos de nível (0-100%) e ligar/desligar do bridge via HTTP REST.

## Funcionalidades

- Controle PWM 10-bit (0-1023) @ 1kHz no GPIO 4 (D2)
- Comandos: nível 0-100% e on/off via bridge
- Dashboard web com barra de nível e botões ligar/desligar
- Descoberta automática do gateway via UDP broadcast
- Registro HTTP no gateway
- Envio periódico de estado (nível + on/off)
- Heartbeat periódico
- Polling de comandos do bridge a cada 2s
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)
- OTA (ArduinoOTA + HTTP OTA)
- Comandos via terminal serial
- LED indicador de status (GPIO 2)
- Persistência de nome em EEPROM

## Hardware

| Componente | GPIO | Função |
|------------|------|--------|
| PWM LED Driver | 4 (D2) | Saída PWM para driver MOSFET/LED |
| LED Built-in | 2 | Indicador de status |

**Nota**: Use um driver MOSFET adequado (ex: IRLZ44N + resistor gate) ou módulo LED driver para controlar a lâmpada. O GPIO 4 fornece apenas sinal PWM 3.3V.

## Faixas de PWM

| Nível | PWM (0-1023) | Estado |
|-------|--------------|--------|
| 0%    | 0            | DESLIGADO |
| 1-100%| 10-1023      | LIGADO (proporcional) |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_NAME "Luz Dimerizavel"
#define PWM_PIN 4
#define PWM_FREQ 1000
#define PWM_RESOLUTION 10
```

## API Bridge

Tipo registrado: `"dimmable"`

Estado enviado:
```json
{"id":"esp8266_xxxxxx","level":50,"on":true}
```

Comandos recebidos (polling `/api/device/commands`):
```json
[{"cluster":"levelcontrol","command":"set_level","data":"75"}]
[{"cluster":"onoff","command":"set_onoff","data":"1"}]
```

## Build (PlatformIO)

```bash
# Build
./build.sh
# ou
pio run -e esp8266

# Flash via USB
./flash.sh [porta]
# ou
pio run -e esp8266 -t upload --upload-port /dev/ttyUSB0

# Monitor serial
./monitor.sh [porta]
# ou
pio device monitor -p /dev/ttyUSB0 -b 115200
```

## OTA (Over-The-Air)

### ArduinoOTA (rede local)
Após conectar no WiFi, acessível via `{device_id}.local` (ex: `esp8266_xxxxxx.local`).

```bash
# PlatformIO
pio run -e esp8266_ota -t upload --upload-port esp8266_xxxxxx.local

# espota.py
espota.py -i esp8266_xxxxxx.local -p 8266 -f .pio/build/esp8266/firmware.bin
```

### HTTP OTA (via browser/dashboard)
POST multipart para `http://<ip_do_esp>/api/ota` com arquivo `firmware.bin`.

### Atalho serial
- `u` — exibe info OTA (hostname, porta, comandos)

## API Local (no ESP8266)

| Rota         | Método | Descrição                    |
|--------------|--------|------------------------------|
| `/`          | GET    | Dashboard web                |
| `/api/state` | GET    | Estado atual (level, on)     |
| `/api/set`   | POST   | Define nível: `{"level":50}` |

## Atalhos de Teclhos de Teclado (Terminal Serial)

- `l` — ler estado atual
- `s` — status do dispositivo
- `r` — restart
- `u` — info OTA
- `h/?` — ajuda

## LED Indicador (GPIO 2)

| Padrão | Significado |
|--------|-------------|
| Aceso fixo | Modo configuração WiFi |
| Pisca rápido (200ms) | WiFi desconectado |
| Pisca lento (2s) | WiFi OK, bridge não descoberto |
| Apagado | Conectado ao bridge |

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0

## Integração com Bridge

O bridge deve suportar tipo `dimmable` (RainMaker lightbulb device). O client:
1. Descobre bridge via UDP porta 5000 (service: "esp-bridge")
2. Registra via POST `/api/device/register` com type="dimmable"
3. Envia estado via POST `/api/device/state` com `{level, on}`
4. Polling comandos via GET `/api/device/commands?id=<device_id>`
5. Recebe `set_level` (cluster: levelcontrol) e `set_onoff` (cluster: onoff)
6. Heartbeat via POST `/api/device/heartbeat`

## Persistência EEPROM

Nome do dispositivo salvo em EEPROM (endereço 0, marker 0xFF). Validação: apenas ASCII imprimível (32-126).