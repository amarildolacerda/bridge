# ESP8266 Gas Detector

Cliente ESP8266 para detector de gás (MQ-2, MQ-5, MQ-7, MQ-135). Envia nível de gás e status de alarme para o bridge via HTTP REST.

## Funcionalidades

- Leitura de sensor de gás via entrada analógica (ADC)
- Alarme por limiar ou pino digital do módulo MQ
- LED indicador de alarme (pisca rápido 200ms)
- Comandos via terminal serial (pressione 'h' para ajuda)
- Descoberta automática do gateway via UDP
- Registro HTTP no gateway
- Envio periódico de telemetria
- Heartbeat periódico
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)

## Hardware

| Componente   | GPIO |
|--------------|------|
| MQ (analogo) | A0   |
| MQ (digital) | D0   |
| LED          | 2    |

O pino digital do módulo MQ dispare em nivel LOW quando a concentração ultrapassa o limiar ajustado no potenciômetro do módulo. Se não usado, o alarme é acionado via software quando `gas_level >= 60%`.

## Faixas de leitura

O ADC 0–1024 é mapeado linearmente para 0–100%.

| % gás     | LED        | Interpretação |
|-----------|------------|---------------|
| 0–15%     | Apagado    | Normal        |
| 15–30%    | Pisca lento (1s) | Atenção |
| 30%+      | Pisca rápido (200ms) | Alarme — vazamento |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_NAME "Detector Gas"
#define GAS_ALERT_THRESHOLD 15
#define GAS_ALARM_THRESHOLD 30
```

## Tipos de sensor MQ suportados

| Sensor | Gás detectado              |
|--------|----------------------------|
| MQ-2   | GLP, propano, metano       |
| MQ-5   | GLP, gás natural           |
| MQ-7   | Monóxido de carbono (CO)   |
| MQ-135 | Qualidade do ar, CO2, fumaca |

## API bridge

Tipo registrado: `"gas"`

Estado enviado:
```json
{"id":"esp8266_xxxxxx","gas_level":42,"alarm":false}
```

## Build (PlatformIO)

```bash
pio run -t upload
pio device monitor
```

## API local (no ESP8266)

| Rota         | Método | Descrição                    |
|--------------|--------|------------------------------|
| `/`          | GET    | Dashboard web                |
| `/api/state` | GET    | Estado atual (gas_level, alarm) |

## Atalhos de teclado (terminal serial)

- `l` — ler sensor agora
- `s` — status do dispositivo
- `r` — restart
- `h/?` — ajuda

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
