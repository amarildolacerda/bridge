#include "sensor_registry.h"
#include "config.h"
#include <EEPROM.h>
#include <Arduino.h>

static virtual_sensor_t s_sensors[MAX_VIRTUAL_SENSORS];
static bool s_initialized = false;

bool sensor_registry_init() {
    EEPROM.begin(EEPROM_SIZE);
    sensor_registry_load();
    EEPROM.end();
    s_initialized = true;
    Serial.printf("[Gateway] Sensor registry initialized: %d paired\n", sensor_registry_count_paired());
    return true;
}

int sensor_registry_find_by_mac(const uint8_t *mac) {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired && mac_equal(s_sensors[i].mac, mac)) {
            return i;
        }
    }
    return -1;
}

int sensor_registry_find_free_slot() {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (!s_sensors[i].paired) {
            return i;
        }
    }
    return -1;
}

virtual_sensor_t* sensor_registry_get(int slot) {
    if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) return nullptr;
    return &s_sensors[slot];
}

int sensor_registry_count_paired() {
    int count = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired) count++;
    }
    return count;
}

int sensor_registry_count_online() {
    int count = 0;
    unsigned long now = millis();
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired && s_sensors[i].online && (now - s_sensors[i].last_seen < SENSOR_TIMEOUT_MS)) {
            count++;
        }
    }
    return count;
}

bool sensor_registry_add(const uint8_t *mac, uint8_t type, uint16_t slot, const char *name) {
    if (slot >= MAX_VIRTUAL_SENSORS) return false;
    if (sensor_registry_find_by_mac(mac) >= 0) return false;

    virtual_sensor_t *s = &s_sensors[slot];
    mac_copy(s->mac, mac);
    s->type = type;
    s->slot = slot;
    s->sequence = 0;
    s->battery_pct = 100;
    s->last_rssi = -127;
    s->last_seen = 0;
    s->paired = true;
    s->online = false;
    memset(&s->state, 0, sizeof(s->state));
    snprintf(s->bridge_device_id, sizeof(s->bridge_device_id), "esp8266_gw_%02X%02X%02X_sensor_%d", mac[3], mac[4], mac[5], slot);
    if (name && strlen(name) > 0) {
        strncpy(s->name, name, sizeof(s->name) - 1);
    } else {
        snprintf(s->name, sizeof(s->name), "Sensor %d", slot + 1);
    }
    s->name[sizeof(s->name) - 1] = '\0';

    sensor_registry_save();
    Serial.printf("[Gateway] Added sensor slot %d: MAC=%02X:%02X:%02X:%02X:%02X:%02X type=%d\n",
                  slot, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], type);
    return true;
}

bool sensor_registry_remove(int slot) {
    if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) return false;
    if (!s_sensors[slot].paired) return false;

    memset(&s_sensors[slot], 0, sizeof(virtual_sensor_t));
    sensor_registry_save();
    Serial.printf("[Gateway] Removed sensor slot %d\n", slot);
    return true;
}

bool sensor_registry_update_state(int slot, const espnow_header_t *header, const uint8_t *payload, size_t payload_len) {
    if (slot < 0 || slot >= MAX_VIRTUAL_SENSORS) return false;
    virtual_sensor_t *s = &s_sensors[slot];
    if (!s->paired) return false;

    s->sequence = header->sequence;
    s->battery_pct = header->battery_pct;
    s->last_rssi = header->rssi;
    s->last_seen = millis();
    s->online = true;

    switch (s->type) {
        case SENSOR_TYPE_TEMP_HUM: {
            if (payload_len >= sizeof(payload_temp_hum_t)) {
                payload_temp_hum_t *p = (payload_temp_hum_t*)payload;
                s->state.temp_hum.temperature = p->temperature;
                s->state.temp_hum.humidity = p->humidity;
            }
            break;
        }
        case SENSOR_TYPE_CONTACT: {
            if (payload_len >= sizeof(payload_contact_t)) {
                payload_contact_t *p = (payload_contact_t*)payload;
                s->state.contact.contact_state = p->contact_state;
                s->state.contact.tamper = p->tamper;
            }
            break;
        }
        case SENSOR_TYPE_MOTION: {
            if (payload_len >= sizeof(payload_motion_t)) {
                payload_motion_t *p = (payload_motion_t*)payload;
                s->state.motion.motion_state = p->motion_state;
                s->state.motion.occupancy_duration = p->occupancy_duration;
            }
            break;
        }
        case SENSOR_TYPE_GAS:
        case SENSOR_TYPE_DHT_GAS: {
            if (payload_len >= sizeof(payload_dht_gas_t)) {
                payload_dht_gas_t *p = (payload_dht_gas_t*)payload;
                s->state.dht_gas.temperature = p->temperature;
                s->state.dht_gas.humidity = p->humidity;
                s->state.dht_gas.gas_level = p->gas_level;
                s->state.dht_gas.alarm = p->alarm;
            } else if (payload_len >= sizeof(payload_gas_t)) {
                payload_gas_t *p = (payload_gas_t*)payload;
                s->state.dht_gas.gas_level = p->gas_level;
                s->state.dht_gas.alarm = p->alarm;
            }
            break;
        }
        case SENSOR_TYPE_RAIN: {
            if (payload_len >= sizeof(payload_rain_t)) {
                payload_rain_t *p = (payload_rain_t*)payload;
                s->state.rain.rain_level = p->rain_level;
                s->state.rain.rain_digital = p->rain_digital;
            }
            break;
        }
        case SENSOR_TYPE_TANK: {
            if (payload_len >= sizeof(payload_tank_t)) {
                payload_tank_t *p = (payload_tank_t*)payload;
                s->state.tank.level_pct = p->level_pct;
                s->state.tank.distance_cm = p->distance_cm;
            }
            break;
        }
    }

    size_t expected = 0;
    switch (s->type) {
        case SENSOR_TYPE_TEMP_HUM: expected = sizeof(payload_temp_hum_t); break;
        case SENSOR_TYPE_CONTACT:  expected = sizeof(payload_contact_t); break;
        case SENSOR_TYPE_MOTION:   expected = sizeof(payload_motion_t); break;
        case SENSOR_TYPE_GAS:      expected = sizeof(payload_gas_t); break;
        case SENSOR_TYPE_DHT_GAS:  expected = sizeof(payload_dht_gas_t); break;
        case SENSOR_TYPE_RAIN:     expected = sizeof(payload_rain_t); break;
        case SENSOR_TYPE_TANK:     expected = sizeof(payload_tank_t); break;
    }
    if (expected && payload_len >= expected + 4)
        memcpy(s->ip, payload + payload_len - 4, 4);

    return true;
}

bool sensor_registry_save() {
    Serial.println("[EEPROM] Saving sensors...");
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        int addr = EEPROM_SENSOR_BASE + i * EEPROM_SENSOR_SIZE;
        uint8_t marker = s_sensors[i].paired ? 0xAA : 0x00;
        EEPROM.write(addr, marker);
        if (s_sensors[i].paired) {
            EEPROM.write(addr + 1, s_sensors[i].type);
            EEPROM.write(addr + 2, s_sensors[i].slot);
            for (int j = 0; j < 6; j++) EEPROM.write(addr + 3 + j, s_sensors[i].mac[j]);
            for (int j = 0; j < 32; j++) {
                if (j < (int)strlen(s_sensors[i].name))
                    EEPROM.write(addr + 9 + j, s_sensors[i].name[j]);
                else
                    EEPROM.write(addr + 9 + j, 0);
            }
            EEPROM.write(addr + 41, 0);
            Serial.printf("[EEPROM] Saved slot %d marker=0x%02X at addr=%d\n", i, marker, addr);
        }
    }
    bool ok = EEPROM.commit();
    EEPROM.end();
    Serial.printf("[EEPROM] commit=%s (%d paired)\n", ok ? "OK" : "FAIL", sensor_registry_count_paired());
    return ok;
}

void sensor_registry_load() {
    Serial.print("[EEPROM] Dump markers:");
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        uint8_t m = EEPROM.read(EEPROM_SENSOR_BASE + i * EEPROM_SENSOR_SIZE);
        Serial.printf(" %02X", m);
    }
    Serial.println();
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        int addr = EEPROM_SENSOR_BASE + i * EEPROM_SENSOR_SIZE;
        uint8_t marker = EEPROM.read(addr);
        if (marker == 0xAA) {
            s_sensors[i].paired = true;
            s_sensors[i].type = EEPROM.read(addr + 1);
            s_sensors[i].slot = EEPROM.read(addr + 2);
            for (int j = 0; j < 6; j++) s_sensors[i].mac[j] = EEPROM.read(addr + 3 + j);
            char name[33] = {0};
            int name_len = 0;
            for (int j = 0; j < 32; j++) {
                uint8_t c = EEPROM.read(addr + 9 + j);
                if (c == 0) break;
                if (c < 32 || c > 126) break;
                name[j] = (char)c;
                name_len = j + 1;
            }
            name[name_len] = 0;
            strncpy(s_sensors[i].name, name, sizeof(s_sensors[i].name) - 1);
            s_sensors[i].sequence = 0;
            s_sensors[i].battery_pct = 100;
            s_sensors[i].last_rssi = -127;
            s_sensors[i].last_seen = 0;
            s_sensors[i].online = false;
            memset(&s_sensors[i].state, 0, sizeof(s_sensors[i].state));
            snprintf(s_sensors[i].bridge_device_id, sizeof(s_sensors[i].bridge_device_id),
                     "esp8266_gw_%02X%02X%02X_sensor_%d",
                     s_sensors[i].mac[3], s_sensors[i].mac[4], s_sensors[i].mac[5], i);
        }
    }
}

void sensor_registry_clear_all() {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        memset(&s_sensors[i], 0, sizeof(virtual_sensor_t));
    }
    sensor_registry_save();
    Serial.println("[Gateway] All sensors cleared");
}

void sensor_registry_print_all() {
    Serial.println("\n=== Sensor Registry ===");
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        if (s_sensors[i].paired) {
            char mac_str[18];
            mac_to_str(s_sensors[i].mac, mac_str, sizeof(mac_str));
            Serial.printf("  Slot %d: %s | MAC=%s | Type=%d | Seq=%u | Batt=%d%% | RSSI=%d | Online=%s\n",
                          i, s_sensors[i].name, mac_str, s_sensors[i].type,
                          s_sensors[i].sequence, s_sensors[i].battery_pct,
                          s_sensors[i].last_rssi, s_sensors[i].online ? "Yes" : "No");
        }
    }
    Serial.printf("Total: %d paired, %d online\n", sensor_registry_count_paired(), sensor_registry_count_online());
    Serial.println("========================\n");
}