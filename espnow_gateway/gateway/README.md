# ESP8266 ESP-NOW Gateway

Gateway ESP-NOW para sensores battery-powered que encaminha dados para o Bridge ESP32 via WiFi.

## Hardware
- **D1 Mini** (ESP-12E/F) com USB para alimentação contínua
- **Pino PAIR_BUTTON**: GPIO0 (D3 - botão boot)
- **Pino STATUS_LED**: GPIO2 (D4 - LED builtin)

## Funcionalidades
- Recebe dados via ESP-NOW (canal 1) de até 20 sensores
- Pareamento via botão físico (3s) ou dashboard web
- Encaminha para Bridge ESP32 via HTTP REST
- Dashboard web em `http://<IP>`
- OTA via ArduinoOTA (`<device_id>.local`)
- Serial commands para debug

## Serial Commands
| Tecla | Ação |
|-------|------|
| `h` / `?` | Ajuda |
| `l` | Listar sensores pareados |
| `p` | Iniciar pareamento (60s) |
| `c` | Limpar TODOS os sensores |
| `r` | Reiniciar |
| `b` | Re-registrar todos no Bridge |
| `s` | Status completo |
| `w` | Forçar portal WiFi |

## Pareamento
1. Pressione botão boot (GPIO0) por 3s OU clique "+ Adicionar Sensor" no dashboard
2. LED builtin pisca rápido = modo pareamento ativo (60s)
3. Ligue/acorde o sensor ESP-NOW
4. LED solidifica por 2s = pareado com sucesso
5. Sensor aparece no dashboard e registra no Bridge

## Configuração WiFi
- Primeira vez: conecta AP `ESP_Gateway_Setup` / `password123`
- Portal em `192.168.4.1`
- Configure Bridge IP/Port opcional
- Salva em EEPROM

## Bridge Integration
- Registra cada sensor virtual como device separado no Bridge
- IDs: `esp8266_<gateway_chipid>_sensor_<slot>`
- Tipos suportados: temperature, contact, occupancy, gas, rain, tanque
- Heartbeat a cada 30s
- Responde a broadcast `re_register` do Bridge

## Build
```bash
cd clients/esp8266_gateway
./build.sh
```

## Flash
```bash
pio run -e esp8266_gateway -t upload --upload-port /dev/ttyUSB0
```

## Monitor
```bash
./monitor.sh
# ou
pio device monitor -e esp8266_gateway --baud 115200
```

## Dashboard
Acesse `http://<IP_DO_GATEWAY>` no navegador:
- Estatísticas gerais
- Lista de sensores com estado, bateria, RSSI, último visto
- Botões: Parear, Re-registrar, Renomear, Remover
- Configuração Bridge IP/Port

## Protocolo ESP-NOW
- Canal: 1 (deve coincidir com WiFi do Bridge)
- Payload binário compacto (< 250 bytes)
- ACK com sequence number para dedup
- Tipos de mensagem: SENSOR_DATA, PAIR_REQUEST, PAIR_RESPONSE, ACK, HEARTBEAT

## Sensor Types
| Type ID | Nome | Campos |
|---------|------|--------|
| 1 | Temp+Hum | temperature, humidity |
| 2 | Contato | contact_state, tamper |
| 3 | Movimento | occupancy, duration |
| 4 | Gás | gas_level, alarm |
| 5 | Chuva | rain_level, rain_digital |
| 6 | Tanque | level_pct, distance_cm |

## EEPROM Layout
```
0x000-0x2CF: 20 sensores x 48 bytes cada
0x300-0x33F: Bridge host (64 bytes)
0x340-0x341: Bridge port (2 bytes)
```

## Troubleshooting
- **Bridge não descoberto**: Verifique se Bridge está no mesmo WiFi, porta UDP 5000
- **Sensor não pareia**: Verifique canal ESP-NOW (deve ser 1), distância < 200m
- **Dados não chegam no Bridge**: Verifique IP/Porta do Bridge no dashboard
- **Gateway reinicia**: Verifique alimentação USB (500mA+), watchdog