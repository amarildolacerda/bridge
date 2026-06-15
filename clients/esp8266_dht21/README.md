# ESP8266 DHT11 Bridge Client

Cliente ESP8266 com sensor DHT11 para o gateway bridge. Envia temperatura e umidade para o gateway via HTTP REST.

## Funcionalidades

- Leitura de temperatura e umidade (DHT11)
- Comandos via terminal serial (pressione 'h' para ajuda)
- Descoberta automática do gateway via UDP
- Registro HTTP no gateway
- Envio periódico de telemetria
- Heartbeat periódico
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)

## Hardware

| Componente | GPIO |
|------------|------|
| DHT11 | 5 (D1) |
| LED | 2 |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_NAME "Temperatura Sala"
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
| `/api/state` | GET | Estado atual (temperatura, umidade) |

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
- `adafruit/DHT sensor library` ^1.4.6
