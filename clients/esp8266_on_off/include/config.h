#pragma once
#include <Arduino.h>

// Device ID é dinâmico: "esp8266_<chip_id>" (gerado via ESP.getChipId() no setup())
#define DEVICE_NAME "Luz Sala"

#define DEVICE_TYPE_ONOFF 1
#define DEVICE_TYPE_TEMPERATURE 2
#define DEVICE_TYPE_CONTACT 3
#define DEVICE_TYPE_OCCUPANCY 4
#define DEVICE_TYPE_DIMMABLE 5

#define DEVICE_TYPE DEVICE_TYPE_ONOFF

#define STATE_UPDATE_INTERVAL 5000  // unused (kept for reference)
#define HEARTBEAT_INTERVAL 60000    // periodic keep-alive (bridge timeout = 120s)
#define TELEMETRY_INTERVAL 30000
#define COMMAND_POLL_INTERVAL 1000

#define DISCOVERY_PORT 5000
#define DISCOVERY_TIMEOUT 30000

#define BRIDGE_HOST "0.0.0.0"
#define BRIDGE_PORT 80

#if DEVICE_TYPE == DEVICE_TYPE_ONOFF || DEVICE_TYPE == DEVICE_TYPE_DIMMABLE
#define RELAY_PIN 4
#define BUTTON_PIN 5
#endif

#if DEVICE_TYPE == DEVICE_TYPE_TEMPERATURE
#define DHT_PIN 4
#endif

#define LED_PIN 2