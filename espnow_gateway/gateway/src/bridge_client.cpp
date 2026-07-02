#include "bridge_client.h"
#include "sensor_registry.h"
#include "config.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

static char s_bridge_host[64] = BRIDGE_HOST_DEFAULT;
static uint16_t s_bridge_port = BRIDGE_PORT_DEFAULT;
static bool s_bridge_discovered = false;
static unsigned long s_last_discovery = 0;
static unsigned long s_last_heartbeat = 0;

static bool is_valid_host() {
    if (strcmp(s_bridge_host, "0.0.0.0") == 0) return false;
    if (strcmp(s_bridge_host, "") == 0) return false;
    return true;
}

#define HTTP_MIN_HEAP 16384

static bool http_post(const char *path, const String &body) {
    if (!is_valid_host()) return false;
    if (ESP.getFreeHeap() < HTTP_MIN_HEAP) {
        static unsigned long last_warn = 0;
        if (millis() - last_warn > 10000) {
            last_warn = millis();
            Serial.printf("[BRIDGE] Heap too low (%d) for %s\n", ESP.getFreeHeap(), path);
        }
        return false;
    }

    IPAddress ip;
    if (!ip.fromString(s_bridge_host)) return false;

    WiFiClient wifi;
    wifi.setTimeout(3000);
    if (!wifi.connect(ip, s_bridge_port)) {
        static unsigned long last_warn = 0;
        if (millis() - last_warn > 30000) {
            last_warn = millis();
            Serial.printf("[BRIDGE] TCP connect failed %s:%d (heap=%d)\n",
                          s_bridge_host, s_bridge_port, ESP.getFreeHeap());
        }
        return false;
    }

    String req = "POST ";
    req += path;
    req += " HTTP/1.1\r\nHost: ";
    req += s_bridge_host;
    req += "\r\nContent-Type: application/json\r\nContent-Length: ";
    req += body.length();
    req += "\r\nConnection: close\r\n\r\n";
    req += body;
    wifi.write(req.c_str(), req.length());

    unsigned long deadline = millis() + 3000;
    String resp;
    while (millis() < deadline) {
        if (wifi.available()) {
            char c = wifi.read();
            resp += c;
            if (resp.endsWith("\r\n\r\n") || resp.length() > 1024) break;
        }
        delay(1);
    }

    bool ok = resp.startsWith("HTTP/1.1 200");
    if (!ok) {
        static unsigned long last_warn = 0;
        if (millis() - last_warn > 30000) {
            last_warn = millis();
            int code_start = resp.indexOf(' ');
            String code_str = (code_start >= 0) ? resp.substring(code_start + 1, code_start + 4) : "???";
            Serial.printf("[BRIDGE] POST %s -> %s\n", path, code_str.c_str());
        }
    }
    wifi.stop();
    return ok;
}

bool bridge_client_load_config() {
    EEPROM.begin(EEPROM_SIZE);
    
    bool valid = false;
    for (int i = 0; i < 63; i++) {
        char c = EEPROM.read(EEPROM_BRIDGE_HOST_OFFSET + i);
        if (c == 0) { valid = true; break; }
        s_bridge_host[i] = c;
    }
    s_bridge_host[63] = '\0';
    
    if (!valid || strlen(s_bridge_host) == 0) {
        strcpy(s_bridge_host, BRIDGE_HOST_DEFAULT);
    }
    
    uint16_t port = 0;
    EEPROM.get(EEPROM_BRIDGE_PORT_OFFSET, port);
    if (port > 0 && port < 65535) {
        s_bridge_port = port;
    }
    
    EEPROM.end();
    
    Serial.printf("[BRIDGE] Config loaded: %s:%d\n", s_bridge_host, s_bridge_port);
    return true;
}

bool bridge_client_save_config(const char *host, uint16_t port) {
    EEPROM.begin(EEPROM_SIZE);
    
    for (int i = 0; i < 64; i++) {
        EEPROM.write(EEPROM_BRIDGE_HOST_OFFSET + i, i < strlen(host) ? host[i] : 0);
    }
    EEPROM.put(EEPROM_BRIDGE_PORT_OFFSET, port);
    EEPROM.commit();
    EEPROM.end();
    
    strcpy(s_bridge_host, host);
    s_bridge_port = port;
    
    Serial.printf("[BRIDGE] Config saved: %s:%d\n", host, port);
    return true;
}

bool bridge_client_discover() {
    Serial.println("[BRIDGE] Starting active discovery...");
    
    WiFiUDP udp;
    udp.begin(BRIDGE_DISCOVERY_PORT);
    
    JsonDocument req;
    req["service"] = "esp-bridge";
    req["discover"] = true;
    
    String payload;
    serializeJson(req, payload);
    
    udp.beginPacket(IPAddress(255, 255, 255, 255), BRIDGE_DISCOVERY_PORT);
    udp.write((const uint8_t*)payload.c_str(), payload.length());
    udp.endPacket();
    
    unsigned long start = millis();
    while (millis() - start < 5000) {
        int packetSize = udp.parsePacket();
        if (packetSize) {
            char buffer[512];
            int len = udp.read(buffer, sizeof(buffer) - 1);
            buffer[len] = '\0';
            
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, buffer);
            if (!err && doc.containsKey("service")) {
                if (strcmp(doc["service"], "esp-bridge") == 0) {
                    const char *host = doc["ip_sta"];
                    int port = doc["http_port"] | 0;
                    if (!port) port = BRIDGE_PORT_DEFAULT;
                    
                    if (host && strlen(host) > 0) {
                        strcpy(s_bridge_host, host);
                        s_bridge_port = port;
                        s_bridge_discovered = true;
                        Serial.printf("[BRIDGE] Discovered: http://%s:%d\n", s_bridge_host, s_bridge_port);
                        return true;
                    }
                }
            }
        }
        delay(10);
    }
    
    Serial.println("[BRIDGE] Discovery timeout");
    return false;
}

void bridge_client_maintain_discovery() {
    if (s_bridge_discovered) return;
    
    unsigned long now = millis();
    if (now - s_last_discovery > 10000) {
        s_last_discovery = now;
        bridge_client_discover();
    }
}

const char* sensor_type_to_string(uint8_t type) {
    switch (type) {
        case SENSOR_TYPE_TEMP_HUM: return "temperature";
        case SENSOR_TYPE_CONTACT: return "contact";
        case SENSOR_TYPE_MOTION: return "occupancy";
        case SENSOR_TYPE_GAS: return "gas";
        case SENSOR_TYPE_DHT_GAS: return "dht_gas";
        case SENSOR_TYPE_RAIN: return "rain";
        case SENSOR_TYPE_TANK: return "tanque";
        default: return "unknown";
    }
}

bool bridge_client_register_sensor(virtual_sensor_t *sensor) {
    if (!s_bridge_discovered) return false;
    
    JsonDocument doc;
    doc["id"] = sensor->bridge_device_id;
    doc["type"] = sensor_type_to_string(sensor->type);
    doc["name"] = sensor->name;
    doc["ip"] = WiFi.localIP().toString();
    doc["parent_id"] = get_gateway_device_id();
    
    String body;
    serializeJson(doc, body);
    
    bool ok = http_post("/api/device/register", body);
    if (ok) {
        sensor->online = true;
        Serial.printf("[BRIDGE] Registered sensor: %s (%s)\n", sensor->bridge_device_id, sensor->name);
    }
    return ok;
}

bool bridge_client_register_all() {
    int count = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strlen(s->bridge_device_id) > 0) {
            if (bridge_client_register_sensor(s)) count++;
        }
    }
    Serial.printf("[BRIDGE] Registered %d sensors\n", count);
    return count > 0;
}

bool bridge_client_send_state(virtual_sensor_t *sensor) {
    if (!s_bridge_discovered) return false;
    
    JsonDocument doc;
    doc["id"] = sensor->bridge_device_id;
    
    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM:
            doc["temperature"] = sensor->state.temp_hum.temperature;
            doc["humidity"] = sensor->state.temp_hum.humidity;
            break;
        case SENSOR_TYPE_CONTACT:
            doc["contact"] = sensor->state.contact.contact_state;
            doc["tamper"] = sensor->state.contact.tamper;
            break;
        case SENSOR_TYPE_MOTION:
            doc["occupancy"] = sensor->state.motion.motion_state;
            doc["duration"] = sensor->state.motion.occupancy_duration;
            break;
        case SENSOR_TYPE_GAS:
            doc["gas_level"] = sensor->state.gas.gas_level;
            doc["alarm"] = sensor->state.gas.alarm;
            break;
        case SENSOR_TYPE_DHT_GAS:
            doc["temperature"] = sensor->state.dht_gas.temperature;
            doc["humidity"] = sensor->state.dht_gas.humidity;
            doc["gas_level"] = sensor->state.dht_gas.gas_level;
            doc["alarm"] = sensor->state.dht_gas.alarm;
            break;
        case SENSOR_TYPE_RAIN:
            doc["rain_level"] = sensor->state.rain.rain_level;
            doc["rain_digital"] = sensor->state.rain.rain_digital;
            break;
        case SENSOR_TYPE_TANK:
            doc["level_pct"] = sensor->state.tank.level_pct;
            doc["distance_cm"] = sensor->state.tank.distance_cm;
            break;
    }
    doc["battery"] = sensor->battery_pct;
    doc["rssi"] = sensor->last_rssi;
    doc["online"] = sensor->online;
    
    String body;
    serializeJson(doc, body);
    
    return http_post("/api/device/state", body);
}

bool bridge_client_send_heartbeat(virtual_sensor_t *sensor) {
    if (!s_bridge_discovered) return false;
    
    String body = "{\"id\":\"" + String(sensor->bridge_device_id) + "\"}";
    return http_post("/api/device/heartbeat", body);
}

void bridge_client_handle_broadcast(const JsonDocument &doc) {
    bool re_reg = doc["re_register"] | false;
    if (re_reg) {
        Serial.println("[BRIDGE] Re-register requested by bridge");
        bridge_client_register_all();
    }
}

const char* bridge_client_get_host() { return s_bridge_host; }
uint16_t bridge_client_get_port() { return s_bridge_port; }
bool bridge_client_is_discovered() { return s_bridge_discovered; }

const char* get_gateway_device_id() {
    static char id[48];
    uint32_t chip_id = ESP.getChipId();
    snprintf(id, sizeof(id), "esp8266_gateway_%06x", chip_id);
    return id;
}

void bridge_client_generate_device_ids() {
    uint32_t chip_id = ESP.getChipId();
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && strlen(s->bridge_device_id) == 0) {
            snprintf(s->bridge_device_id, sizeof(s->bridge_device_id), 
                     "esp8266_%06x_sensor_%d", chip_id, i);
        }
    }
}