#pragma once

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID "kcasa"
#define WIFI_PASSWORD "3938373635"

// ── Bridge (HTTP REST) ───────────────────────────────────────────────────────
#define BRIDGE_HOST "0.0.0.0"
#define BRIDGE_PORT 80

// ── Device Identity ──────────────────────────────────────────────────────────
// Device ID é dinâmico: "esp8266_<chip_id>" (gerado via ESP.getChipId() no setup())
#define DEVICE_TYPE "tanque"
#define DEVICE_NAME "Caixa D'Agua"

// ── HC-SR04 Ultrasonic Pins ──────────────────────────────────────────────────
#define TRIG_PIN 5 // GPIO5 (D1)
#define ECHO_PIN 4 // GPIO4 (D2)

// ── Tank Dimensions (cm) ─────────────────────────────────────────────────────
#define TANK_HEIGHT_CM 200.0 // altura total do tanque
#define SENSOR_OFFSET_CM 5.0 // distancia do sensor ate o topo do tanque
// Distancia medida quando vazio = TANK_HEIGHT_CM + SENSOR_OFFSET_CM - (altura do fundo ate o bico saida)
// Para simplificar: sensor no topo, mede distancia ate a agua
// Quando cheio: distancia pequena (tampo do tanque ate agua = SENSOR_OFFSET_CM)
// Quando vazio: distancia grande (tampo ate fundo = TANK_HEIGHT_CM)

// ── Level Thresholds (%)
#define LEVEL_EMPTY_MAX 10
#define LEVEL_LOW_MAX 30
#define LEVEL_HALF_MAX 70
#define LEVEL_HIGH_MAX 90
#define LEVEL_FULL_MIN 90

// ── Timing ───────────────────────────────────────────────────────────────────
#define READ_INTERVAL_MS 10000 // ler sensor a cada 10s
#define SEND_INTERVAL_MS 30000 // enviar estado a cada 30s
#define SONAR_TIMEOUT_US 30000 // 30ms ~ 5m de alcance maximo
