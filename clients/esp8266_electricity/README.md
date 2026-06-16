# ESP8266 Electricity Meter

Cliente ESP8266 para **Grove Electricity Sensor** (TA12-200 / SCT-013). Mede corrente AC até 5A via clamp não invasivo.

## Funcionalidades

- Medição de corrente AC (mA RMS) via sensor de corrente TA12-200/SCT-013
- LED indicador de status
- Comandos via terminal serial (pressione 'h' para ajuda)
- Descoberta automática do gateway via UDP
- Registro HTTP no gateway
- Heartbeat periódico
- Servidor web embarcado com dashboard
- Portal de configuração WiFi (WiFiManager)
- OTA via ArduinoOTA

## Hardware

### Grove Electricity Sensor (TA12-200)

| Componente          | GPIO |
|---------------------|------|
| Sensor (SIG)        | A0   |
| LED                 | 2    |

### Conexão

```
ESP8266 NodeMCU    Grove Electricity Sensor
-------------------------------------------
5V/Vin             VCC (vermelho)
GND                GND (preto)
A0                 SIG (amarelo)
-                  NC (branco) - não conectar
```

> O sensor precisa de alimentação 5V (use o pino Vin do NodeMCU).

### Circuito de Bias

O sensor TA12-200 gera um sinal AC puro. Em alguns casos pode ser necessário um circuito de polarização para centrar o sinal em ~1.65V (metade do Vref do ADC):

```
3.3V ──┬─ 10kΩ ──┬─ A0
       │         │
     100nF       └── 10kΩ ── GND
       │
   SIG ─┘
```

Se o sensor já tiver bias interno (versões Grove), pode conectar direto.

## Fórmula de cálculo

- ADC 0-1024 mapeia para 0-3.3V
- Tensão de pico: Vpeak = sensor_max / 1024 * 3.3
- Corrente de pico: Ipeak(mA) = Vpeak / 800 * 2000 * 1000
- Corrente RMS: Irms = Ipeak / 1.414

## Faixas de corrente

| mA RMS     | Classificação |
|------------|---------------|
| 0–199 mA   | Baixa         |
| 200–999 mA | Média         |
| 1000+ mA   | Alta          |

## Configuração

Edite `include/config.h`:

```cpp
#define DEVICE_NAME "Medidor Energia"
#define AC_VREF 3.3f
#define SAMPLING_MS 500
```

## OTA

```bash
pio run -t upload --upload-port esp8266_xxxxxx.local
```

## API bridge

Tipo registrado: `"electricity"`

Estado enviado:
```json
{"id":"esp8266_xxxxxx","current_ma":350}
```

## API local (no ESP8266)

| Rota         | Método | Descrição                    |
|--------------|--------|------------------------------|
| `/`          | GET    | Dashboard web                |
| `/api/state` | GET    | Estado atual (current_ma)    |

## Atalhos de teclado (terminal serial)

- `l` — ler sensor agora
- `s` — status do dispositivo
- `r` — restart
- `u` — info OTA
- `h/?` — ajuda

## Dependências (PlatformIO)

- `bblanchon/ArduinoJson` ^7.3.1
- `tzapu/WiFiManager` ^2.0.0
