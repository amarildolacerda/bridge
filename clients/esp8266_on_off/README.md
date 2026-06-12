# ESP8266 Bridge Client

Cliente ESP8266 para o [ESP RainMaker Gateway Bridge](https://github.com/amarildolacerda/bridge). Conecta sensores e atuadores ao gateway via WiFi, registrando-se como dispositivo bridged no RainMaker/Alexa.

## Funcionalidades

- Descoberta automática do gateway via UDP
- Registro HTTP no gateway
- Polling de comandos (on/off, nível, restart)
- Envio de estado (com mudança detectada automática)
- Heartbeat periódico
- Servidor web embarcado com dashboard
- Botão físico para toggle (configurável)
- Portal de configuração WiFi (WiFiManager)
- Suporte a múltiplos tipos de dispositivo via `#define`

## Tipos de dispositivo

| Tipo | `DEVICE_TYPE` | Descrição |
|------|---------------|-----------|
| On/Off | `DEVICE_TYPE_ONOFF` | Relé liga/desliga |
| Dimmer | `DEVICE_TYPE_DIMMABLE` | PWM ajustável |
| Temperatura | `DEVICE_TYPE_TEMPERATURE` | Sensor DHT (simulado) |
| Contato | `DEVICE_TYPE_CONTACT` | Sensor magnético |
| Ocupação | `DEVICE_TYPE_OCCUPANCY` | Sensor de presença |

## Hardware (padrão)

| Componente | GPIO |
|------------|------|
| Relé | 4 |
| Botão | 5 |
| LED | 2 |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_ID "esp8266_living_room"
#define DEVICE_NAME "Luz Sala"
#define DEVICE_TYPE DEVICE_TYPE_ONOFF
```

## Build (PlatformIO)

```bash
pio run -t upload
pio device monitor
```

## API local (no ESP8266)

| Rota | Método | Descrição |
|------|--------|-----------|
| `/` | GET | Dashboard web |
| `/api/state` | GET | Estado atual (JSON) |
| `/api/on` | POST | Ligar |
| `/api/off` | POST | Desligar |
| `/api/toggle` | POST | Inverter estado |

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
