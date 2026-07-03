#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include "espnow_protocol.h"
#include <Arduino.h>

#define MAX_VIRTUAL_SENSORS 20

typedef struct {
    uint8_t mac[6];
    uint8_t type;
    uint16_t sequence;
    uint8_t slot;
    char name[32];
    char bridge_device_id[48];
    uint8_t battery_pct;
    int16_t last_rssi;
    unsigned long last_seen;
    bool paired;
    bool online;
    uint8_t ip[4];
    union {
        struct { float temperature; float humidity; } temp_hum;
        struct { uint8_t contact_state; uint8_t tamper; } contact;
        struct { uint8_t motion_state; uint8_t occupancy_duration; } motion;
        struct { uint16_t gas_level; uint8_t alarm; } gas;
        struct { float temperature; float humidity; uint16_t gas_level; uint8_t alarm; } dht_gas;
        struct { uint8_t rain_level; uint8_t rain_digital; } rain;
        struct { uint16_t level_pct; uint16_t distance_cm; } tank;
    } state;
} virtual_sensor_t;

bool sensor_registry_init();
int sensor_registry_find_by_mac(const uint8_t *mac);
int sensor_registry_find_free_slot();
virtual_sensor_t* sensor_registry_get(int slot);
int sensor_registry_count_paired();
int sensor_registry_count_online();
bool sensor_registry_add(const uint8_t *mac, uint8_t type, uint16_t slot, const char *name);
bool sensor_registry_remove(int slot);
bool sensor_registry_update_state(int slot, const espnow_header_t *header, const uint8_t *payload, size_t payload_len);
bool sensor_registry_save();
void sensor_registry_load();
void sensor_registry_clear_all();
void sensor_registry_print_all();

#endif