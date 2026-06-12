# ESP RainMaker Gateway Bridge

Gateway bridge para dispositivos IoT utilizando ESP RainMaker. Permite que dispositivos como sensores e atuadores se registrem e se comuniquem através de um gateway central, integrando-os ao ecossistema RainMaker da Alexa.

## Funcionalidades

- **Registro de dispositivos** via HTTP e descoberta UDP
- **Bridge RainMaker** — dispositivos bridged aparecem como dispositivos nativos no app RainMaker/Alexa
- **Painel web** embutido com dashboard e WebSocket em tempo real
- **API REST** completa para registro, remoção, estado e comandos
- **Persistência em NVS** — dispositivos registrados são restaurados após reboot
- **Heartbeat** — monitora quais dispositivos estão online
- **Comandos** — fila de comandos (on/off, nível) para dispositivos bridged

## Tipos de dispositivo suportados

| Tipo | Descrição |
|------|-----------|
| `onoff` | Ligar/desligar |
| `dimmable` | Intensidade ajustável |
| `temperature` | Sensor de temperatura |
| `humidity` | Sensor de umidade |
| `contact` | Sensor de contato (porta/janela) |
| `occupancy` | Sensor de ocupação |
| `light_sensor` | Sensor de luminosidade |
| `tanque` | Nível de tanque |

## API REST

| Rota | Método | Descrição |
|------|--------|-----------|
| `/api/device/register` | POST | Registrar dispositivo |
| `/api/device/remove` | POST | Remover dispositivo |
| `/api/device/state` | POST | Atualizar estado |
| `/api/device/commands` | GET/POST | Obter comandos pendentes |
| `/api/device/info` | GET | Informações do dispositivo |
| `/api/device/heartbeat` | POST | Heartbeat |
| `/api/devices` | GET | Listar dispositivos |
| `/api/gateway/info` | GET | Informações do gateway |
| `/api/ping` | GET | Ping |
| `/api/gateway/reset` | POST | Reiniciar gateway |
| `/` | GET | Dashboard web |
| `/ws` | GET | WebSocket |

## Estrutura

```
main/               — Firmware do gateway (ESP-IDF)
matter/             — Projeto Matter (ESP Matter)
clients/            — Firmware dos dispositivos clientes
  esp32_mqtt/
  esp8266_dh11/
  esp8266_on_off/
  esp8266_tanque/
test/               — Scripts de teste Python
```

## Build

```bash
idf.py build
idf.py flash monitor
```
