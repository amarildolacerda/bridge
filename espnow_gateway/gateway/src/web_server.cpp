#include "web_server.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include "bridge_client.h"
#include "config.h"
#include "pages.h"
#include <ESP8266WebServer.h>
#include <uri/UriBraces.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

static ESP8266WebServer s_server(80);
static bool s_wifi_config_mode = false;
static unsigned long s_wifi_config_start = 0;

void web_server_init() {
    s_server.on("/", HTTP_GET, []() {
        s_server.send(200, "text/html", FPSTR(PAGE_DASHBOARD));
    });
    
    s_server.on("/api/info", HTTP_GET, []() {
        JsonDocument doc;
        doc["paired_count"] = sensor_registry_count_paired();
        doc["online_count"] = sensor_registry_count_online();
        doc["rx_total"] = espnow_get_rx_count();
        doc["ack_total"] = espnow_get_ack_count();
        doc["crc_errors"] = espnow_get_crc_errors();
        doc["uptime_ms"] = millis();
        doc["pairing_mode"] = espnow_is_pairing();
        doc["pairing_window_sec"] = PAIRING_WINDOW_MS / 1000;
        doc["bridge_host"] = bridge_client_get_host();
        doc["bridge_port"] = bridge_client_get_port();
        doc["bridge_discovered"] = bridge_client_is_discovered();
        char mac_buf[18];
        mac_to_str(espnow_get_gateway_mac(), mac_buf, sizeof(mac_buf));
        doc["gateway_mac"] = mac_buf;
        doc["gateway_id"] = get_gateway_device_id();
        doc["fw_version"] = FW_VERSION;
        
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });
    
    s_server.on("/api/sensors", HTTP_GET, []() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        
        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired) {
                JsonObject obj = arr.add<JsonObject>();
                obj["slot"] = s->slot;
                char mac_str[18];
                mac_to_str(s->mac, mac_str, sizeof(mac_str));
                obj["mac"] = mac_str;
                for (int j = 0; j < 6; j++) obj["mac_bytes"].add(s->mac[j]);
                obj["type"] = s->type;
                obj["type_name"] = sensor_type_to_string(s->type);
                obj["name"] = s->name;
                obj["bridge_device_id"] = s->bridge_device_id;
                obj["sequence"] = s->sequence;
                obj["battery_pct"] = s->battery_pct;
                obj["last_rssi"] = s->last_rssi;
                obj["last_seen"] = (s->online && s->last_seen > 0) ? (long)(millis() - s->last_seen) : -1;
                obj["online"] = s->online;
                obj["paired"] = s->paired;
                if (s->ip[0] || s->ip[1] || s->ip[2] || s->ip[3]) {
                    char ip_str[16];
                    sprintf(ip_str, "%d.%d.%d.%d", s->ip[0], s->ip[1], s->ip[2], s->ip[3]);
                    obj["ip"] = ip_str;
                }
                
                JsonObject state = obj["state"].to<JsonObject>();
                switch (s->type) {
                    case SENSOR_TYPE_TEMP_HUM:
                        state["temperature"] = s->state.temp_hum.temperature;
                        state["humidity"] = s->state.temp_hum.humidity;
                        break;
                    case SENSOR_TYPE_CONTACT:
                        state["contact"] = s->state.contact.contact_state;
                        state["tamper"] = s->state.contact.tamper;
                        break;
                    case SENSOR_TYPE_MOTION:
                        state["occupancy"] = s->state.motion.motion_state;
                        state["duration"] = s->state.motion.occupancy_duration;
                        break;
                    case SENSOR_TYPE_GAS:
                        state["gas_level"] = s->state.gas.gas_level;
                        state["alarm"] = s->state.gas.alarm;
                        break;
                    case SENSOR_TYPE_RAIN:
                        state["rain_level"] = s->state.rain.rain_level;
                        state["rain_digital"] = s->state.rain.rain_digital;
                        break;
                    case SENSOR_TYPE_TANK:
                        state["level_pct"] = s->state.tank.level_pct;
                        state["distance_cm"] = s->state.tank.distance_cm;
                        break;
                    case SENSOR_TYPE_DHT_GAS:
                        state["temperature"] = s->state.dht_gas.temperature;
                        state["humidity"] = s->state.dht_gas.humidity;
                        state["gas_level"] = s->state.dht_gas.gas_level;
                        state["alarm"] = s->state.dht_gas.alarm;
                        break;
                }
            }
        }
        
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });
    
    s_server.on("/api/pair/start", HTTP_POST, []() {
        if (espnow_is_pairing()) {
            s_server.send(409, "application/json", "{\"error\":\"already pairing\"}");
        } else if (espnow_start_pairing()) {
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            s_server.send(400, "application/json", "{\"error\":\"max sensors reached\"}");
        }
    });
    
    s_server.on("/api/pair/stop", HTTP_POST, []() {
        espnow_stop_pairing();
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/api/clear", HTTP_POST, []() {
        sensor_registry_clear_all();
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    s_server.on("/api/broadcast", HTTP_POST, []() {
        bridge_client_register_all();
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    s_server.on(UriBraces("/api/sensor/{}/{}"), HTTP_POST, []() {
        int slot = s_server.pathArg(0).toInt();
        String action = s_server.pathArg(1);
        
        if (action == "name") {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
            if (err || !doc.containsKey("name")) {
                s_server.send(400, "application/json", "{\"error\":\"invalid json or missing name\"}");
                return;
            }
            const char *name = doc["name"];
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                s_server.send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            strncpy(s->name, name, sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            sensor_registry_save();
            bridge_client_register_sensor(s);
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else if (action == "remove") {
            if (sensor_registry_remove(slot)) {
                s_server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                s_server.send(404, "application/json", "{\"error\":\"sensor not found\"}");
            }
        } else {
            s_server.send(404, "application/json", "{\"error\":\"unknown action\"}");
        }
    });
    
    s_server.on("/api/config/bridge", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("host") || !doc.containsKey("port")) {
            s_server.send(400, "application/json", "{\"error\":\"host and port required\"}");
            return;
        }
        const char *host = doc["host"];
        uint16_t port = doc["port"];
        if (bridge_client_save_config(host, port)) {
            bridge_client_discover();
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            s_server.send(500, "application/json", "{\"error\":\"save failed\"}");
        }
    });
    
    s_server.on("/api/restart", HTTP_POST, []() {
        s_server.send(200, "application/json", "{\"status\":\"restarting\"}");
        delay(500);
        ESP.restart();
    });
    
    s_server.on("/api/ota", HTTP_POST, []() {
        if (!Update.hasError()) {
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
            delay(500);
            ESP.restart();
        } else {
            s_server.send(500, "application/json", "{\"status\":\"error\"}");
        }
    }, []() {
        HTTPUpload &upload = s_server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[OTA] Update started: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
            if (!Update.begin(upload.totalSize)) Update.printError(Serial);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                Update.printError(Serial);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true))
                Serial.printf("[OTA] Success: %d bytes\n", upload.totalSize);
            else
                Update.printError(Serial);
        }
    });
    
    s_server.begin();
    Serial.println("[WEB] Server started on port 80");
}

void web_server_loop() {
    s_server.handleClient();
    ArduinoOTA.handle();
    
    if (s_wifi_config_mode && millis() - s_wifi_config_start > 300000) {
        Serial.println("[WIFI] Config portal timeout, restarting...");
        ESP.restart();
    }
}

bool web_server_wifi_setup(bool force_portal) {
    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(20);
    
    if (!force_portal && WiFi.SSID().length() > 0) {
        wifiManager.setTimeout(180);
        wifiManager.setConnectRetries(3);
        Serial.printf("[WIFI] Connecting to saved: %s\n", WiFi.SSID().c_str());
        if (wifiManager.autoConnect()) {
            Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            s_wifi_config_mode = false;
            return true;
        }
        Serial.println("[WIFI] Failed to connect to saved WiFi");
    }
    
    Serial.println("[WIFI] Starting config portal...");
    s_wifi_config_mode = true;
    s_wifi_config_start = millis();
    wifiManager.setConfigPortalTimeout(300);
    
    WiFiManagerParameter custom_bridge_host("bridge_host", "Bridge IP", bridge_client_get_host(), 64);
    WiFiManagerParameter custom_bridge_port("bridge_port", "Bridge Port", String(bridge_client_get_port()).c_str(), 6);
    wifiManager.addParameter(&custom_bridge_host);
    wifiManager.addParameter(&custom_bridge_port);
    
    if (wifiManager.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS)) {
        if (strlen(custom_bridge_host.getValue()) > 0) {
            bridge_client_save_config(custom_bridge_host.getValue(), atoi(custom_bridge_port.getValue()));
        }
        s_wifi_config_mode = false;
        return true;
    }
    
    Serial.println("[WIFI] Config portal timeout");
    s_wifi_config_mode = false;
    return false;
}

void web_server_maintain_wifi() {
    if (WiFi.status() == WL_CONNECTED) return;
    
    static unsigned long last_attempt = 0;
    if (millis() - last_attempt < 30000) return;
    last_attempt = millis();
    
    Serial.println("[WIFI] Reconnecting...");
    WiFi.begin();
    unsigned long start = millis();
    while (millis() - start < 15000) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WIFI] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
            return;
        }
        delay(500);
    }
}