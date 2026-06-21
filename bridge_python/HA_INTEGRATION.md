# Integração com Home Assistant

## Visão Geral

O **bridge_python** funciona como um **add-on do Home Assistant**. Ele substitui o bridge ESP32 físico, mantendo compatibilidade total com os clients ESP8266 existentes.

A integração usa **MQTT Discovery**: quando um device se registra no bridge, o add-on publica automaticamente entidades no tópico `homeassistant/` — o Home Assistant descobre e cria os dispositivos nativamente, sem configurar nada manualmente.

## Pré-requisitos

- Home Assistant (qualquer instalação: OS, Container, Core + supervised)
- Add-on **Mosquitto broker** instalado e rodando no HA
- Integração MQTT configurada no Home Assistant (Settings → Add-ons → Mosquitto → Configuration)

## Instalação como Add-on

### 1. Adicionar o repositório local

```bash
# No servidor do Home Assistant (ou via Samba/SSH), crie um link para o add-on:
mkdir -p /addons/esp32-bridge-python
cp -r bridge_python/* /addons/esp32-bridge-python/
```

Ou, se estiver no mesmo host do Home Assistant:

```bash
ln -s /caminho/para/bridge_python /addons/esp32-bridge-python
```

### 2. Recarregar repositórios no HA

- Vá em **Settings → Add-ons → Supervisor → Add-on Store**
- Clique nos **três pontinhos** (canto superior direito) → **Reload**
- O add-on **"ESP32 Bridge Python"** aparecerá na lista

### 3. Instalar

- Clique no add-on e depois em **Install**

### 4. Configurar

| Opção | Padrão | Descrição |
|-------|--------|-----------|
| `mqtt_host` | `core-mosquitto` | Host do broker MQTT |
| `mqtt_port` | `1883` | Porta do broker |
| `mqtt_user` | vazio | Usuário MQTT |
| `mqtt_pass` | vazio | Senha MQTT |
| `log_level` | `info` | debug, info, warn, error |

O valor padrão `core-mosquitto` funciona automaticamente quando o Mosquitto está instalado como add-on do HA.

### 5. Iniciar

- Clique em **Start**
- Verifique os logs: **"MQTT connected to core-mosquitto:1883"**

## MQTT Discovery

Assim que devices ESP8266 se registram via HTTP ou UDP discovery, o bridge publica configurações no formato:

```
homeassistant/<platform>/<device_id>/<entity_name>/config
```

E os estados são publicados em:

```
homeassistant/<platform>/<device_id>/<entity_name>/state
```

O Home Assistant descobre automaticamente cada entidade e a adiciona como dispositivo nativo.

### Mapeamento Device Type → Entidades HA

| Device Type | Entidade(s) | Platform | Device Class | Unidade |
|-------------|-------------|----------|-------------|---------|
| `onoff` | `power` | `switch` | — | — |
| `dimmable` | `light` | `light` | — | — |
| `temperature` | `temperature` | `sensor` | `temperature` | °C |
| `temperature` | `humidity` | `sensor` | `humidity` | % |
| `humidity` | `humidity` | `sensor` | `humidity` | % |
| `contact` | `contact` | `binary_sensor` | `door` | — |
| `occupancy` | `occupancy` | `binary_sensor` | `occupancy` | — |
| `light_sensor` | `light` | `sensor` | — | lx |
| `tanque` | `level` | `sensor` | — | % |
| `gas` | `alarm` | `binary_sensor` | `gas` | — |
| `gas` | `gas_level` | `sensor` | — | % |
| `rain` | `rain_digital` | `binary_sensor` | `moisture` | — |
| `rain` | `rain_level` | `sensor` | — | % |
| `electricity` | `current` | `sensor` | `current` | mA |

### Dispositivos com Controle (switch, light)

Para `onoff` e `dimmable`, o bridge escuta comandos no tópico `.../set`. Quando você alterna um switch ou light pela UI do HA, o bridge:

1. Recebe o comando MQTT (`true` / `false`)
2. Adiciona o comando à fila do device
3. O client ESP8266 pega o comando via `GET /api/device/commands`
4. Executa a ação (liga/desliga relé, ajusta brilho)

## Verificando o Funcionamento

1. **Logs do add-on**: devem mostrar `"Published MQTT discovery for <device_id>"`
2. **Home Assistant → Settings → Devices & Services → MQTT**: os dispositivos aparecem como "ESP-HA Bridge"
3. **Dashboard web do bridge**: acesse `http://<ha_ip>:8080/` para ver devices registrados em tempo real

## Solução de Problemas

| Problema | Causa provável | Solução |
|----------|---------------|---------|
| Add-on não inicia | Mosquitto não está rodando | Instale e inicie o Mosquitto add-on |
| Log: "MQTT connection failed" | MQTT_HOST errado ou Mosquitto não acessível | Verifique se `mqtt_host` está configurado corretamente |
| Entidades não aparecem no HA | Integração MQTT não configurada | Settings → Devices & Services → Add Integration → MQTT |
| Device não fica online | Client ESP8266 sem WiFi | Verifique o client físico |
| Nenhum device registrado | Clients não descobriram o bridge | Envie broadcast no dashboard web (botão Broadcast) |
| Comandos não chegam no device | Device offline ou fila de comandos cheia | Verifique heartbeat do device nos logs |

## Compatibilidade

O bridge_python é compatível com todos os clients ESP8266 existentes:

- `clients/esp8266_gas/`
- `clients/esp8266_dht21/`
- `clients/esp8266_chuva/`

Nenhuma modificação nos clients é necessária — eles se comunicam via HTTP e UDP discovery exatamente como fazem com o bridge ESP32 físico.
