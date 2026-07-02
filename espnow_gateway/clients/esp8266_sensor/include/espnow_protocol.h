#ifndef ESPNOW_PROTOCOL_H
#define ESPNOW_PROTOCOL_H

#include <stdint.h>

#define ESPNOW_PROTOCOL_VERSION 1
#define ESPNOW_MAX_PAYLOAD 250
#define ESPNOW_SEQUENCE_MAX 65535

typedef enum {
    ESPNOW_MSG_SENSOR_DATA = 0x01,
    ESPNOW_MSG_PAIR_REQUEST = 0x02,
    ESPNOW_MSG_PAIR_RESPONSE = 0x03,
    ESPNOW_MSG_ACK = 0x04,
    ESPNOW_MSG_HEARTBEAT = 0x05,
    ESPNOW_MSG_OTA_TRIGGER = 0x06,
} espnow_msg_type_t;

typedef enum {
    SENSOR_TYPE_TEMP_HUM = 1,
    SENSOR_TYPE_CONTACT = 2,
    SENSOR_TYPE_MOTION = 3,
    SENSOR_TYPE_GAS = 4,
    SENSOR_TYPE_RAIN = 5,
    SENSOR_TYPE_TANK = 6,
} sensor_type_t;

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t sensor_type;
    uint8_t battery_pct;
    int16_t rssi;
    uint8_t payload_len;
    uint8_t payload[ESPNOW_MAX_PAYLOAD - 18];
} espnow_header_t;

typedef struct __attribute__((packed)) {
    float temperature;
    float humidity;
} payload_temp_hum_t;

typedef struct __attribute__((packed)) {
    uint8_t contact_state;
    uint8_t tamper;
} payload_contact_t;

typedef struct __attribute__((packed)) {
    uint8_t motion_state;
    uint8_t occupancy_duration;
} payload_motion_t;

typedef struct __attribute__((packed)) {
    uint16_t gas_level;
    uint8_t alarm;
} payload_gas_t;

typedef struct __attribute__((packed)) {
    uint8_t rain_level;
    uint8_t rain_digital;
} payload_rain_t;

typedef struct __attribute__((packed)) {
    uint16_t level_pct;
    uint16_t distance_cm;
} payload_tank_t;

typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t status;
    uint16_t assigned_slot;
} espnow_ack_t;

typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t sensor_type;
    uint8_t firmware_version[4];
    char device_name[16];
} espnow_pair_request_t;

typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint16_t sequence;
    uint8_t sensor_mac[6];
    uint8_t gateway_mac[6];
    uint8_t status;
    uint16_t assigned_slot;
} espnow_pair_response_t;

#define PAIR_STATUS_OK 0
#define PAIR_STATUS_FULL 1
#define PAIR_STATUS_DENIED 2

static inline void mac_to_str(const uint8_t *mac, char *buf, size_t len) {
    snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline bool mac_equal(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

static inline void mac_copy(uint8_t *dst, const uint8_t *src) {
    memcpy(dst, src, 6);
}

#endif