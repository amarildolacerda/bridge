#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include "bridge_client.h"
#include "web_server.h"
#include "ota.h"

static const char *TAG = "esp8266_gateway";

static unsigned long s_start_time = 0;
static unsigned long s_last_telemetry = 0;
static unsigned long s_last_serial_check = 0;

void print_help() {
    Serial.println("\n=== Comandos ===");
    Serial.println("  h/?  - Esta ajuda");
    Serial.println("  l    - Listar sensores pareados");
    Serial.printf("  p    - Iniciar modo pareamento (%lus)\n", PAIRING_WINDOW_MS / 1000);
    Serial.println("  c    - Limpar TODOS os sensores");
    Serial.println("  r    - Reiniciar");
    Serial.println("  b    - Broadcast re-register para Bridge");
    Serial.println("  s    - Status do gateway");
    Serial.println("  w    - Forçar portal WiFi");
    Serial.println("================\n");
}

void handle_serial() {
    if (Serial.available() <= 0) return;
    char c = Serial.read();
    
    switch (c) {
        case 'h':
        case 'H':
        case '?':
            print_help();
            break;
            
        case 'l':
        case 'L':
            sensor_registry_print_all();
            break;
            
        case 'p':
        case 'P':
            if (espnow_start_pairing()) {
                Serial.println("Modo pareamento iniciado. LED piscando...");
            } else {
                Serial.println("Falha: máximo de sensores atingido ou já em pareamento");
            }
            break;
            
        case 'c':
        case 'C':
            sensor_registry_clear_all();
            Serial.println("Todos os sensores removidos");
            break;
            
        case 'r':
        case 'R':
            Serial.println("Reiniciando...");
            delay(100);
            ESP.restart();
            break;
            
        case 'b':
        case 'B':
            bridge_client_register_all();
            break;
            
        case 's':
        case 'S': {
            Serial.printf("\n=== Status Gateway ===\n");
            Serial.printf("Device ID: %s\n", get_gateway_device_id());
            char mac_buf[18];
            mac_to_str(espnow_get_gateway_mac(), mac_buf, sizeof(mac_buf));
            Serial.printf("MAC: %s\n", mac_buf);
            Serial.printf("FW: %s\n", FW_VERSION);
            Serial.printf("Uptime: %lu s\n", millis() / 1000);
            Serial.printf("Sensores: %d pareados, %d online\n", 
                          sensor_registry_count_paired(), sensor_registry_count_online());
            Serial.printf("ESP-NOW: RX=%lu ACK=%lu CRC_ERR=%lu\n",
                          espnow_get_rx_count(), espnow_get_ack_count(), espnow_get_crc_errors());
            Serial.printf("Bridge: %s:%d (%s)\n", 
                          bridge_client_get_host(), bridge_client_get_port(),
                          bridge_client_is_discovered() ? "descoberto" : "não descoberto");
            Serial.printf("WiFi: %s ch=%d (RSSI: %d dBm)\n",
                          WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "desconectado",
                          WiFi.channel(), WiFi.RSSI());
            Serial.printf("Pareamento: %s\n", espnow_is_pairing() ? "ATIVO" : "inativo");
            Serial.printf("========================\n\n");
            break;
        }
            
        case 'w':
        case 'W':
            Serial.println("Forçando portal WiFi...");
            web_server_wifi_setup(true);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    s_start_time = millis();
    
    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, HIGH);
    pinMode(PAIR_BUTTON_GPIO, INPUT_PULLUP);
    
    Serial.printf("\n");
    Serial.printf("============================================\n");
    Serial.printf("  ESP8266 ESP-NOW Gateway %s\n", FW_VERSION);
    Serial.printf("  Device: %s\n", get_gateway_device_id());
    Serial.printf("============================================\n");
    
    sensor_registry_init();
    bridge_client_load_config();
    bridge_client_generate_device_ids();
    
    ota_init(get_gateway_device_id());
    
    if (!web_server_wifi_setup(false)) {
        Serial.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }
    
    espnow_handler_init();
    web_server_init();
    
    if (bridge_client_discover()) {
        bridge_client_register_all();
    } else {
        Serial.printf("[%s] Bridge não descoberto, aguardando...\n", TAG);
    }
    
    Serial.printf("============================================\n");
    Serial.printf("  Pronto! 'h' para ajuda\n");
    Serial.printf("  Dashboard: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("============================================\n\n");
}

void loop() {
    handle_serial();
    
    static unsigned long last_button_check = 0;
    static unsigned long press_start = 0;
    if (millis() - last_button_check > 50) {
        last_button_check = millis();
        if (digitalRead(PAIR_BUTTON_GPIO) == LOW) {
            if (press_start == 0) press_start = millis();
            else if (millis() - press_start > 3000) {
                if (!espnow_is_pairing()) {
                    espnow_start_pairing();
                }
                press_start = 0;
                while (digitalRead(PAIR_BUTTON_GPIO) == LOW) delay(10);
            }
        } else {
            press_start = 0;
        }
    }
    
    web_server_loop();
    web_server_maintain_wifi();
    espnow_handler_loop();
    bridge_client_maintain_discovery();
    
    unsigned long now = millis();
    
    if (now - s_last_telemetry > 30000) {
        s_last_telemetry = now;
        Serial.printf("[%s] Uptime=%lus RX=%lu ACK=%lu Paired=%d Online=%d\n",
                      TAG, now / 1000, espnow_get_rx_count(), espnow_get_ack_count(),
                      sensor_registry_count_paired(), sensor_registry_count_online());
        
        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired && s->online) {
                bridge_client_send_heartbeat(s);
            }
        }
    }
    
    delay(1);
}