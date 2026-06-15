#pragma once
#include <Arduino.h>

// Device ID é dinâmico: "esp8266_<chip_id>" (gerado via ESP.getChipId() no setup())
#define DEVICE_NAME "Temperatura Sala"

#define DEVICE_TYPE_ONOFF 1
#define DEVICE_TYPE_TEMPERATURE 2
#define DEVICE_TYPE_CONTACT 3
#define DEVICE_TYPE_OCCUPANCY 4
#define DEVICE_TYPE_DIMMABLE 5

#define DEVICE_TYPE DEVICE_TYPE_TEMPERATURE

#define STATE_UPDATE_INTERVAL 60000
#define TELEMETRY_INTERVAL 30000
#define COMMAND_POLL_INTERVAL 100

#define DISCOVERY_PORT 5000
#define DISCOVERY_TIMEOUT 30000

#define BRIDGE_HOST "0.0.0.0"
#define BRIDGE_PORT 80

#if DEVICE_TYPE == DEVICE_TYPE_ONOFF || DEVICE_TYPE == DEVICE_TYPE_DIMMABLE
#define RELAY_PIN 4
#define BUTTON_PIN 5
#endif

#define DHT_PIN 4
#define DHT_TYPE DHT21
#define LED_PIN 2
