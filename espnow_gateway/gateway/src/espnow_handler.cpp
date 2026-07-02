#include "espnow_handler.h"
#include "config.h"
#include "sensor_registry.h"
#include "bridge_client.h"
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <Arduino.h>

static bool s_pairing_mode = false;
static unsigned long s_pairing_start = 0;
static uint8_t s_gateway_mac[6];
static unsigned long s_last_heartbeat = 0;
static unsigned long s_rx_count = 0;
static unsigned long s_ack_count = 0;
static unsigned long s_crc_errors = 0;

#define PENDING_PAIR_MAX 5
typedef struct {
    bool active;
    uint8_t mac[6];
    uint8_t sensor_type;
    uint16_t sequence;
    char name[32];
} pending_pair_t;
static pending_pair_t s_pending_pairs[PENDING_PAIR_MAX];

#define PENDING_STATE_MAX 5
static uint8_t s_pending_state_slots[PENDING_STATE_MAX];
static int s_pending_state_head = 0;
static int s_pending_state_tail = 0;

static void queue_bridge_state(int slot) {
    int next = (s_pending_state_head + 1) % PENDING_STATE_MAX;
    if (next == s_pending_state_tail) return;
    s_pending_state_slots[s_pending_state_head] = slot;
    s_pending_state_head = next;
}

static void send_ack(const uint8_t *mac, uint16_t sequence, uint8_t status, uint8_t slot);
static void send_pair_response(const uint8_t *mac, uint16_t sequence, uint16_t slot);

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
    if (!data || len < 2) {
        s_crc_errors++;
        return;
    }

    s_rx_count++;
    uint8_t msg_type = data[0];
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    Serial.printf("[ESP-NOW] RX msg_type=%d len=%d from=%s pairing=%d\n",
                  msg_type, len, mac_str, s_pairing_mode);

    switch (msg_type) {
        case ESPNOW_MSG_PAIR_REQUEST: {
            if (len < sizeof(espnow_pair_request_t)) { s_crc_errors++; return; }
            if (!s_pairing_mode) { Serial.printf("[ESP-NOW] Pair request ignored (not pairing)\n"); return; }
            if (sensor_registry_find_by_mac(mac) >= 0) return;

            espnow_pair_request_t *req = (espnow_pair_request_t*)data;

            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (!s_pending_pairs[i].active) {
                    mac_copy(s_pending_pairs[i].mac, mac);
                    s_pending_pairs[i].sensor_type = req->sensor_type;
                    s_pending_pairs[i].sequence = req->sequence;
                    snprintf(s_pending_pairs[i].name, sizeof(s_pending_pairs[i].name), "%s %d",
                             (req->sensor_type == SENSOR_TYPE_TEMP_HUM) ? "Temp+Hum" :
                             (req->sensor_type == SENSOR_TYPE_CONTACT) ? "Contato" :
                             (req->sensor_type == SENSOR_TYPE_MOTION) ? "Movimento" :
                             (req->sensor_type == SENSOR_TYPE_GAS) ? "Gas" :
                             (req->sensor_type == SENSOR_TYPE_DHT_GAS) ? "DHT+Gas" :
                             (req->sensor_type == SENSOR_TYPE_RAIN) ? "Chuva" : "Tanque",
                             i + 1);
                    s_pending_pairs[i].active = true;
                    break;
                }
            }
            break;
        }

        case ESPNOW_MSG_SENSOR_DATA:
        case ESPNOW_MSG_HEARTBEAT: {
            if (len < sizeof(espnow_header_t)) { s_crc_errors++; return; }
            espnow_header_t *hdr = (espnow_header_t*)data;

            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { s_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(mac);

            if (msg_type == ESPNOW_MSG_SENSOR_DATA) {
                if (slot < 0) {
                    send_ack(mac, hdr->sequence, PAIR_STATUS_DENIED, 0xFF);
                    return;
                }
                sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
                send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                s_ack_count++;
                queue_bridge_state(slot);
            } else {
                if (slot >= 0) {
                    sensor_registry_get(slot)->last_seen = millis();
                    sensor_registry_get(slot)->online = true;
                    send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                }
            }
            break;
        }

        default:
            s_crc_errors++;
            break;
    }
}

void send_ack(const uint8_t *mac, uint16_t sequence, uint8_t status, uint8_t slot) {
    espnow_ack_t ack = {
        .msg_type = ESPNOW_MSG_ACK,
        .sequence = sequence,
        .sensor_mac = {0},
        .status = status,
        .assigned_slot = slot
    };
    mac_copy(ack.sensor_mac, mac);
    
    esp_now_send((uint8_t*)mac, (uint8_t*)&ack, sizeof(ack));
}

void send_pair_response(const uint8_t *mac, uint16_t sequence, uint16_t slot) {
    espnow_pair_response_t resp = {
        .msg_type = ESPNOW_MSG_PAIR_RESPONSE,
        .sequence = sequence,
        .sensor_mac = {0},
        .gateway_mac = {0},
        .status = PAIR_STATUS_OK,
        .assigned_slot = slot
    };
    mac_copy(resp.sensor_mac, mac);
    mac_copy(resp.gateway_mac, s_gateway_mac);
    
    esp_now_send((uint8_t*)mac, (uint8_t*)&resp, sizeof(resp));
}

bool espnow_handler_init() {
    memset(s_pending_pairs, 0, sizeof(s_pending_pairs));
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(s_gateway_mac);
    
    if (esp_now_init() != 0) {
        Serial.println("[ESP-NOW] Init failed");
        return false;
    }
    
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(espnow_recv_cb);
    
    Serial.printf("[ESP-NOW] Initialized, MAC: %02X:%02X:%02X:%02X:%02X:%02X WiFi ch=%d\n",
                  s_gateway_mac[0], s_gateway_mac[1], s_gateway_mac[2],
                  s_gateway_mac[3], s_gateway_mac[4], s_gateway_mac[5],
                  WiFi.channel());
    return true;
}

void espnow_handler_loop() {
    if (s_pairing_mode && millis() - s_pairing_start > PAIRING_WINDOW_MS) {
        s_pairing_mode = false;
        digitalWrite(STATUS_LED_GPIO, HIGH);
        Serial.println("[ESP-NOW] Pairing mode timeout");
    }

    for (int i = 0; i < PENDING_PAIR_MAX; i++) {
        if (!s_pending_pairs[i].active) continue;

        s_pending_pairs[i].active = false;
        int free_slot = sensor_registry_find_free_slot();

        if (free_slot < 0) {
            send_ack(s_pending_pairs[i].mac, s_pending_pairs[i].sequence, PAIR_STATUS_FULL, 0xFF);
            continue;
        }

        sensor_registry_add(s_pending_pairs[i].mac, s_pending_pairs[i].sensor_type,
                            free_slot, s_pending_pairs[i].name);
        send_pair_response(s_pending_pairs[i].mac, s_pending_pairs[i].sequence, free_slot);
        if (bridge_client_is_discovered())
            bridge_client_register_sensor(sensor_registry_get(free_slot));

        Serial.printf("[ESP-NOW] Paired sensor slot %d: %02X:%02X:%02X:%02X:%02X:%02X type=%d\n",
                      free_slot,
                      s_pending_pairs[i].mac[0], s_pending_pairs[i].mac[1],
                      s_pending_pairs[i].mac[2], s_pending_pairs[i].mac[3],
                      s_pending_pairs[i].mac[4], s_pending_pairs[i].mac[5],
                      s_pending_pairs[i].sensor_type);
    }

    while (s_pending_state_tail != s_pending_state_head) {
        int slot = s_pending_state_slots[s_pending_state_tail];
        s_pending_state_tail = (s_pending_state_tail + 1) % PENDING_STATE_MAX;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired && bridge_client_is_discovered())
            bridge_client_send_state(s);
    }

    if (millis() - s_last_heartbeat > HEARTBEAT_INTERVAL_MS) {
        s_last_heartbeat = millis();

        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired) {
                unsigned long elapsed = millis() - s->last_seen;
                if (s->online && elapsed > SENSOR_TIMEOUT_MS) {
                    s->online = false;
                    Serial.printf("[ESP-NOW] Sensor slot %d offline (last seen %lu ms ago)\n", i, elapsed);
                }
            }
        }
    }
}

bool espnow_start_pairing() {
    if (sensor_registry_count_paired() >= MAX_VIRTUAL_SENSORS) {
        Serial.println("[ESP-NOW] Max sensors reached");
        return false;
    }
    s_pairing_mode = true;
    s_pairing_start = millis();
    digitalWrite(STATUS_LED_GPIO, LOW);
    Serial.printf("[ESP-NOW] Pairing mode started (%lus)\n", PAIRING_WINDOW_MS / 1000);
    return true;
}

void espnow_stop_pairing() {
    s_pairing_mode = false;
    digitalWrite(STATUS_LED_GPIO, HIGH);
    Serial.println("[ESP-NOW] Pairing mode stopped");
}

bool espnow_is_pairing() {
    return s_pairing_mode;
}

unsigned long espnow_get_rx_count() { return s_rx_count; }
unsigned long espnow_get_ack_count() { return s_ack_count; }
unsigned long espnow_get_crc_errors() { return s_crc_errors; }
uint8_t* espnow_get_gateway_mac() { return s_gateway_mac; }