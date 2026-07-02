# ESP-NOW Gateway

Gateway ESP8266 que recebe dados de sensores via ESP-NOW e os registra como devices virtuais no Bridge Python via HTTP.

## Estrutura

```
├── gateway/           # Firmware ESP8266 (PlatformIO)
│   ├── include/       # Headers
│   ├── src/           # Código fonte
│   ├── platformio.ini # Config PlatformIO
│   ├── build.sh       # Build
│   └── monitor.sh     # Monitor serial
├── bridge_python/     # Bridge Python (submodule)
└── README.md
```

## Build

```sh
cd gateway
./build.sh
```

## Submodule

```sh
git submodule update --init --recursive
```

## Funcionamento

1. Gateway recebe dados de sensores via ESP-NOW
2. Registra cada sensor como device virtual no Bridge via HTTP
3. Bridge publica dados no MQTT / Home Assistant
