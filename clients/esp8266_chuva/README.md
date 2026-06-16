# ESP8266 Rain Sensor

Cliente ESP8266 para sensor de chuva. Envia nível de chuva para o bridge via HTTP REST.

## Funcionalidades

- Leitura de sensor de chuva via entrada analógica (ADC)
- LED indicador de status
- Comandos via terminal serial (pressione 'h' para ajuda)
- Descoberta automática do gateway via UDP
- Registro HTTP no gateway
- Heartbeat periódico
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)
- OTA via ArduinoOTA

## Hardware

| Componente          | GPIO |
|---------------------|------|
| Sensor (analógico)  | A0   |
| Sensor (digital)    | 16   |
| LED                 | 2    |

## Faixas de leitura

O ADC 0–1024 é mapeado linearmente para 0–100%.

| % chuva   | Classificação |
|-----------|---------------|
| 0–10%     | Seco          |
| 11–40%    | Chuviscando   |
| 41–70%    | Chovendo      |
| 71–100%   | Chuva forte   |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_NAME "Sensor Chuva"
#define RAIN_DRY_THRESHOLD 90
#define RAIN_WET_THRESHOLD 30
```

## OTA

```bash
pio run -t upload --upload-port esp8266_xxxxxx.local
```

## API bridge

Tipo registrado: `"rain"`

Estado enviado:
```json
{"id":"esp8266_xxxxxx","rain_level":42,"rain_digital":false}
```

## API local (no ESP8266)

| Rota         | Método | Descrição                    |
|--------------|--------|------------------------------|
| `/`          | GET    | Dashboard web                |
| `/api/state` | GET    | Estado atual (rain_level)    |

## Atalhos de teclado (terminal serial)

- `l` — ler sensor agora
- `s` — status do dispositivo
- `r` — restart
- `u` — info OTA
- `h/?` — ajuda

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
