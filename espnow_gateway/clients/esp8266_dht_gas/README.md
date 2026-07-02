# ESP8266 DHT22 + MQ-2 Client (ESP-NOW)

Cliente ESP8266 com sensor DHT22 (temperatura/umidade) e MQ-2 (gás). Comunica com o **ESP-NOW Gateway** via ESP-NOW, que encaminha os dados ao bridge via HTTP.

## Funcionalidades

- Leitura de temperatura e umidade (DHT22)
- Leitura de gás (MQ-2) via entrada analógica
- Alarme por limiar via software + LEDs indicadores (alerta e alarme)
- LED de status (WiFi, gateway, config portal)
- Comandos via terminal serial (pressione 'h' para ajuda)
- Pareamento automático com gateway ESP-NOW
- Envio de dados dos sensores via ESP-NOW
- Heartbeat periódico via ESP-NOW
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)
- OTA via ArduinoOTA e upload HTTP (`/api/ota`)

## Hardware

| Componente     | GPIO |
|----------------|------|
| DHT22          | 4    |
| MQ-2 (analog)  | A0   |
| MQ-2 (digital) | 16   |
| LED status     | 2    |
| LED alerta     | 12   |
| LED alarme     | 5    |

## Faixas de leitura

### Gás (ADC 0–1024 mapeado para 0–100%)

| % gás   | LED alerta           | LED alarme          | Interpretação     |
|---------|----------------------|---------------------|-------------------|
| 0–14%   | Apagado              | Apagado             | Normal            |
| 15–29%  | Pisca lento (1s)     | Apagado             | Atenção           |
| 30%+    | Aceso                | Pisca rápido (200ms)| Alarme — vazamento|

## Configuração

Edite `include/config.h` ou use build_flags no platformio.ini:

```cpp
#define DEVICE_NAME "DHT+Gas ESP-NOW"
#define DHT_PIN 4
#define GAS_ALERT_THRESHOLD 15
#define GAS_ALARM_THRESHOLD 30
#define ESPNOW_CHANNEL 1
```

## Build (PlatformIO)

```bash
pio run -t upload
pio device monitor
```

## API local (no ESP8266)

| Rota              | Método | Descrição                                         |
|-------------------|--------|---------------------------------------------------|
| `/`               | GET    | Dashboard web                                     |
| `/api/state`      | GET    | Estado atual (temperature, humidity, gas_level, alarm) |
| `/api/pin?gpio=N` | GET    | Estado digital do pino GPIO N                     |
| `/api/pin`        | POST   | Controla pino: `{"gpio":N,"state":1|0}`           |
| `/api/ota`        | POST   | Upload de firmware via OTA (multipart)            |

## ESP-NOW Protocol

O cliente implementa o protocolo `espnow_protocol.h` compartilhado:

1. **Pairing**: envia `ESPNOW_MSG_PAIR_REQUEST` (broadcast), gateway responde com `ESPNOW_MSG_PAIR_RESPONSE`
2. **Dados**: envia `ESPNOW_MSG_SENSOR_DATA` com payload combinado (temp/hum/gas)
3. **Heartbeat**: envia `ESPNOW_MSG_HEARTBEAT` periodicamente
4. **ACK**: gateway responde com `ESPNOW_MSG_ACK` para todas as mensagens

MAC do gateway é salvo em EEPROM para persistência entre reboots.

## OTA

Via ArduinoOTA (hostname: `{device_id}.local`, porta 8266):

```bash
pio run -t upload --upload-port esp8266_xxxxxx.local
```

Via upload HTTP (`/api/ota`):

```bash
curl -F "firmware=@.pio/build/nodemcuv2/firmware.bin" http://esp8266_xxxxxx.local/api/ota
```

## Atalhos de teclado (terminal serial)

- `l` — ler sensores agora
- `s` — status do dispositivo
- `r` — restart
- `t` — testar pinos do DHT22
- `p` — resetar par e tentar parear com gateway
- `u` — info OTA
- `h/?` — ajuda

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
- `adafruit/DHT sensor library` ^1.4.6
