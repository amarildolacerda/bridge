#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "pages.h"

static const char *TAG = "esp8266-dimmable";

static WiFiClient s_wifi;
static HTTPClient s_http;
static WiFiUDP s_udp;

static unsigned long s_last_state_update = 0;
static unsigned long s_last_telemetry_update = 0;
static unsigned long s_last_reconnect_attempt = 0;
static unsigned long s_last_broadcast_check = 0;
static unsigned long s_last_bridge_reconnect = 0;
static unsigned long s_last_command_poll = 0;

static bool s_bridge_connected = false;

static int s_level = 0;
static bool s_on = false;
static int s_battery = 100;
static unsigned long s_start_time = 0;
static unsigned long s_last_send_ms = 0;

static char s_device_id[32];
static char s_device_name[48] = DEVICE_NAME;

static char s_bridge_host[64] = BRIDGE_HOST;
static uint16_t s_bridge_port = BRIDGE_PORT;
static bool s_bridge_discovered = false;
static bool s_pending_register_state = false;

static bool s_wifi_configuration_mode = false;
static unsigned long s_wifi_config_start_time = 0;

static ESP8266WebServer s_server(80);

static const char *get_device_type_string(void)
{
    return "dimmable";
}

#define EEPROM_NAME_ADDR 0
#define EEPROM_NAME_MAX 48

static void save_device_name(const char *name)
{
    EEPROM.begin(128);
    EEPROM.write(EEPROM_NAME_ADDR, 0xFF);
    for (int i = 0; i < EEPROM_NAME_MAX - 1; i++) {
        EEPROM.write(EEPROM_NAME_ADDR + 1 + i, name[i]);
        if (name[i] == '\0') break;
    }
    EEPROM.write(EEPROM_NAME_ADDR + EEPROM_NAME_MAX, '\0');
    EEPROM.commit();
    EEPROM.end();
}

static bool is_valid_name(const char *s)
{
    if (!s || s[0] == '\0') return false;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

static void load_device_name(void)
{
    EEPROM.begin(128);
    uint8_t marker = EEPROM.read(EEPROM_NAME_ADDR);
    if (marker == 0xFF) {
        char buf[EEPROM_NAME_MAX];
        for (int i = 0; i < EEPROM_NAME_MAX - 1; i++) {
            buf[i] = EEPROM.read(EEPROM_NAME_ADDR + 1 + i);
            if (buf[i] == '\0') break;
        }
        buf[EEPROM_NAME_MAX - 1] = '\0';
        if (is_valid_name(buf)) {
            strncpy(s_device_name, buf, sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
        }
    }
    EEPROM.end();
}

static bool http_post(const char *path, const String &body)
{
    s_http.begin(s_wifi, String("http://") + s_bridge_host + ":" + s_bridge_port + path);
    s_http.addHeader("Content-Type", "application/json");
    s_http.addHeader("Connection", "close");
    s_http.setTimeout(5000);
    int code = s_http.POST(body);
    bool ok = (code == 200);
    s_bridge_connected = ok;
    if (!ok)
    {
        static unsigned long last_warn = 0;
        if (millis() - last_warn > 30000)
        {
            last_warn = millis();
            Serial.printf("[%s] POST %s -> %d\n", TAG, path, code);
        }
    }
    s_http.end();
    return ok;
}

static bool http_get(const char *path, String &response)
{
    s_http.begin(s_wifi, String("http://") + s_bridge_host + ":" + s_bridge_port + path);
    s_http.addHeader("Connection", "close");
    s_http.setTimeout(505000);
    int code = s_http.GET();
    bool ok = (code == 200);
    if (ok) {
        response = s_http.getString();
    } else {
        static unsigned long last_warn = 0;
        if (millis() - last_warn > 30000)
        {
            last_warn = millis();
            Serial.printf("[%s] GET %s -> %d\n", TAG, path, code);
        }
    }
    s_http.end();
    return ok;
}

static bool register_device(void)
{
    String body;
    {
        JsonDocument doc;
        doc["id"] = s_device_id;
        doc["type"] = get_device_type_string();
        doc["name"] = s_device_name;
        doc["ip"] = WiFi.localIP().toString();
        serializeJson(doc, body);
    }
    Serial.printf("[%s] Registering device: %s\n", TAG, s_device_id);
    bool ok = http_post("/api/device/register", body);
    if (ok)
    {
        Serial.printf("[%s] Device registered successfully\n", TAG);
    }
    else
    {
        Serial.printf("[%s] Failed to register device\n", TAG);
    }
    return ok;
}

static void send_heartbeat(void)
{
    if (!s_bridge_discovered || !s_bridge_connected)
        return;
    s_http.begin(s_wifi, String("http://") + s_bridge_host + ":" + s_bridge_port + "/api/device/heartbeat");
    s_http.addHeader("Content-Type", "application/json");
    s_http.addHeader("Connection", "close");
    s_http.setTimeout(3000);
    String body = "{\"id\":\"" + String(s_device_id) + "\"}";
    s_http.POST(body);
    s_http.end();
}

static void send_state(bool force)
{
    if (!s_bridge_discovered || !s_bridge_connected)
        return;

    static int last_level = -1;
    static bool last_on = false;
    bool changed = false;
    if (force) {
        changed = true;
    } else {
        if (s_on != last_on)
            changed = true;
        else if (abs(s_level - last_level) >= STATE_SEND_THRESHOLD)
            changed = true;
    }
    if (!changed)
        return;

    String body;
    {
        JsonDocument doc;
        doc["id"] = s_device_id;
        doc["level"] = s_level;
        doc["on"] = s_on;
        serializeJson(doc, body);
    }

    if (http_post("/api/device/state", body))
    {
        s_last_send_ms = millis();
        last_level = s_level;
        last_on = s_on;
        Serial.printf("[%s] level=%d%% on=%s\n", TAG, s_level, s_on ? "true" : "false");
    }
}

static void poll_commands(void)
{
    if (!s_bridge_discovered || !s_bridge_connected)
        return;
    unsigned long now = millis();
    if (now - s_last_command_poll < 2000)
        return;
    s_last_command_poll = now;

    String response;
    String path = "/api/device/commands?id=" + String(s_device_id);
    if (http_get(path.c_str(), response))
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);
        if (!error && doc.is<JsonArray>())
        {
            for (JsonObject cmd : doc.as<JsonArray>())
            {
                const char *cluster = cmd["cluster"] | "";
                const char *command = cmd["command"] | "";
                const char *data = cmd["data"] | "";

                Serial.printf("[%s] Command received: %s/%s data=%s\n", TAG, cluster, command, data);

                if (strcmp(cluster, "levelcontrol") == 0 && strcmp(command, "set_level") == 0)
                {
                    int level = atoi(data);
                    level = constrain(level, 0, 100);
                    s_level = level;
                    s_on = (level > 0);
                    analogWrite(PWM_PIN, map(s_level, 0, 100, 0, 1023));
                    send_state(true);
                }
                else if (strcmp(cluster, "onoff") == 0 && strcmp(command, "set_onoff") == 0)
                {
                    bool on = (strcmp(data, "1") == 0 || strcmp(data, "true") == 0);
                    s_on = on;
                    if (!s_on) {
                        s_level = 0;
                    } else if (s_level == 0) {
                        s_level = 100;
                    }
                    analogWrite(PWM_PIN, map(s_level, 0, 100, 0, 1023));
                    send_state(true);
                }
            }
        }
    }
}

static void maintain_bridge_discovery(void)
{
    unsigned long now = millis();
    if (now - s_last_broadcast_check < 100)
        return;
    s_last_broadcast_check = now;

    int packet_size = s_udp.parsePacket();
    if (packet_size)
    {
        char buffer[512];
        int len = s_udp.read(buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, buffer);
        if (!error && doc.containsKey("service"))
        {
            if (strcmp(doc["service"], "esp-bridge") == 0)
            {
                const char *host = doc["ip_sta"];
                int port = doc["http_port"] | 0;
                if (!port)
                    port = BRIDGE_PORT;
                if (host && strlen(host) > 0)
                {
                    if (strcmp(s_bridge_host, host) != 0 || s_bridge_port != port)
                    {
                        strcpy(s_bridge_host, host);
                        s_bridge_port = port;
                        s_bridge_discovered = true;
                        Serial.printf("[%s] Bridge discovered: http://%s:%d\n", TAG, s_bridge_host, s_bridge_port);
                    }
                }
                bool re_reg = doc["re_register"] | false;
                if (re_reg)
                {
                    Serial.printf("[%s] Re-register requested by bridge\n", TAG);
                    register_device();
                    s_pending_register_state = true;
                }
            }
        }
    }

    static unsigned long last_active_discovery = 0;
    unsigned long discovery_interval = s_bridge_discovered ? 60000 : 10000;
    if (now - last_active_discovery > discovery_interval)
    {
        bool should_discover = !s_bridge_discovered;
        if (s_bridge_discovered && !s_bridge_connected)
            should_discover = true;
        if (should_discover)
        {
            last_active_discovery = now;
            JsonDocument req;
            req["service"] = "esp-bridge";
            req["discover"] = true;
            req["id"] = s_device_id;
            String payload;
            serializeJson(req, payload);
            s_udp.beginPacket(IPAddress(255, 255, 255, 255), DISCOVERY_PORT);
            s_udp.write((const uint8_t *)payload.c_str(), payload.length());
            s_udp.endPacket();
            Serial.printf("[%s] Discovery request sent\n", TAG);
        }
    }
}

static bool discover_bridge(void)
{
    JsonDocument req;
    req["service"] = "esp-bridge";
    req["discover"] = true;
    req["id"] = s_device_id;
    String payload;
    serializeJson(req, payload);

    s_udp.beginPacket(IPAddress(255, 255, 255, 255), DISCOVERY_PORT);
    s_udp.write((const uint8_t *)payload.c_str(), payload.length());
    s_udp.endPacket();
    Serial.printf("[%s] Active discovery request sent\n", TAG);

    unsigned long start = millis();
    while (millis() - start < 5000)
    {
        int packet_size = s_udp.parsePacket();
        if (packet_size)
        {
            char buffer[512];
            int len = s_udp.read(buffer, sizeof(buffer) - 1);
            buffer[len] = '\0';

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, buffer);
            if (!error && doc.containsKey("service"))
            {
                if (strcmp(doc["service"], "esp-bridge") == 0)
                {
                    const char *host = doc["ip_sta"];
                    if (host && strlen(host) > 0)
                    {
                        strcpy(s_bridge_host, host);
                        s_bridge_port = doc["http_port"] | BRIDGE_PORT;
                        s_bridge_discovered = true;
                        Serial.printf("[%s] Bridge discovered: %s:%d\n", TAG, s_bridge_host, s_bridge_port);
                        return true;
                    }
                }
            }
        }
        delay(10);
    }

    if (strcmp(BRIDGE_HOST, "0.0.0.0") != 0)
    {
        Serial.printf("[%s] Bridge discovery timeout, using configured: %s:%d\n", TAG, s_bridge_host, s_bridge_port);
        s_bridge_discovered = true;
        return true;
    }
    Serial.printf("[%s] Bridge discovery timeout, no fallback IP configured\n", TAG);
    return false;
}

static bool wifi_setup(bool force_config_portal = false)
{
    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(20);

    if (!force_config_portal && WiFi.SSID() != "")
    {
        wifiManager.setTimeout(180);
        wifiManager.setConnectRetries(3);
        Serial.printf("[%s] Connecting to saved WiFi: %s\n", TAG, WiFi.SSID().c_str());
        if (wifiManager.autoConnect())
        {
            Serial.printf("[%s] WiFi connected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            s_wifi_configuration_mode = false;
            return true;
        }
        Serial.printf("[%s] Failed to connect to saved WiFi\n", TAG);
    }

    Serial.printf("[%s] Starting configuration portal...\n", TAG);
    s_wifi_configuration_mode = true;
    s_wifi_config_start_time = millis();
    wifiManager.setConfigPortalTimeout(300);

    WiFiManagerParameter custom_bridge_host("bridge_host", "Bridge IP", s_bridge_host, 64);
    WiFiManagerParameter custom_bridge_port("bridge_port", "Bridge Port", String(s_bridge_port).c_str(), 6);
    wifiManager.addParameter(&custom_bridge_host);
    wifiManager.addParameter(&custom_bridge_port);

    WiFiManagerParameter custom_dev_name("dev_name", "Device Name", s_device_name, 48);
    wifiManager.addParameter(&custom_dev_name);

    if (wifiManager.startConfigPortal("ESP8266_Dimmable", "password123"))
    {
        if (strlen(custom_bridge_host.getValue()) > 0)
        {
            strcpy(s_bridge_host, custom_bridge_host.getValue());
            s_bridge_port = atoi(custom_bridge_port.getValue());
            if (s_bridge_port == 0)
                s_bridge_port = BRIDGE_PORT;
        }
        if (strlen(custom_dev_name.getValue()) > 0 && strcmp(s_device_name, custom_dev_name.getValue()) != 0) {
            strncpy(s_device_name, custom_dev_name.getValue(), sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
            save_device_name(s_device_name);
        }
        s_wifi_configuration_mode = false;
        return true;
    }

    Serial.printf("[%s] Configuration portal timed out\n", TAG);
    s_wifi_configuration_mode = false;
    return false;
}

static void maintain_wifi_connection(void)
{
    if (WiFi.status() == WL_CONNECTED)
        return;
    unsigned long now = millis();
    if (now - s_last_reconnect_attempt < 30000)
        return;
    s_last_reconnect_attempt = now;

    Serial.printf("[%s] WiFi disconnected. Reconnecting...\n", TAG);
    WiFi.begin();
    unsigned long connect_start = millis();
    while (millis() - connect_start < 15000)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("[%s] Reconnected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            return;
        }
        delay(500);
    }

    static unsigned long last_config_attempt = 0;
    if (now - last_config_attempt > 300000)
    {
        last_config_attempt = now;
        wifi_setup(true);
    }
}

static void init_hardware(void)
{
    pinMode(PWM_PIN, OUTPUT);
    analogWriteFreq(PWM_FREQ);
    analogWriteRange(1023);
    analogWrite(PWM_PIN, 0);

#ifdef BUTTON_PIN
    pinMode(BUTTON_PIN, INPUT_PULLUP);
#endif

#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif
}

static void check_config_portal_timeout(void)
{
    if (s_wifi_configuration_mode && (millis() - s_wifi_config_start_time > 600000))
    {
        Serial.printf("[%s] Config portal timeout. Restarting...\n", TAG);
        ESP.restart();
    }
}

static void handle_root(void)
{
    s_server.send(200, "text/html", FPSTR(PAGE_DASHBOARD));
}

static void handle_api_state(void)
{
    String json;
    {
        JsonDocument doc;
        doc["level"] = s_level;
        doc["on"] = s_on;
        doc["battery"] = s_battery;
        doc["device_id"] = s_device_id;
        doc["device_name"] = s_device_name;
        doc["bridge_connected"] = s_bridge_connected;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        if (s_last_send_ms) doc["last_send_s"] = (millis() - s_last_send_ms) / 1000;
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}

static void handle_api_set(void)
{
    if (s_server.method() != HTTP_POST) {
        s_server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
        return;
    }
    String body = s_server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    int level = doc["level"] | 0;
    level = constrain(level, 0, 100);
    s_level = level;
    s_on = (level > 0);
    analogWrite(PWM_PIN, map(s_level, 0, 100, 0, 1023));
    send_state(true);

    JsonDocument resp;
    resp["status"] = "ok";
    resp["level"] = s_level;
    resp["on"] = s_on;
    String json;
    serializeJson(resp, json);
    s_server.send(200, "application/json", json);
}

static void handle_serial(void)
{
    if (Serial.available() <= 0)
        return;
    char c = Serial.read();
    switch (c)
    {
        case 'R':
        case 'r':
            ESP.restart();
            break;
        case 'l':
        case 'L':
            Serial.printf("\n--- Leitura forçada ---\n");
            Serial.printf("  Nivel:     %d %%\n", s_level);
            Serial.printf("  Estado:    %s\n", s_on ? "LIGADO" : "DESLIGADO");
            Serial.printf("  PWM:       %d / 1023\n", map(s_level, 0, 100, 0, 1023));
            Serial.printf("  Bateria:   %d %%\n", s_battery);
            if (s_bridge_discovered && s_bridge_connected)
            {
                s_last_state_update = 0;
                send_state(true);
            }
            else
            {
                Serial.printf("  (bridge desconectado)\n");
            }
            Serial.printf("------------------------\n\n");
            break;
        case 'u':
        case 'U':
            Serial.printf("\n--- OTA ---\n");
            Serial.printf("  Hostname: %s.local\n", s_device_id);
            Serial.printf("  Port:     8266 (ArduinoOTA)\n");
            Serial.printf("  PlatformIO CLI:\n");
            Serial.printf("    pio run -t upload --upload-port %s.local\n", s_device_id);
            Serial.printf("  espota.py:\n");
            Serial.printf("    espota.py -i %s.local -p 8266 -f firmware.bin\n", s_device_id);
            Serial.printf("-------------\n\n");
            break;
        case 'h':
        case 'H':
        case '?':
            Serial.printf("\n--- Comandos ---\n");
            Serial.printf("  l    - ler estado agora\n");
            Serial.printf("  r    - reset\n");
            Serial.printf("  s    - status do dispositivo\n");
            Serial.printf("  u    - info OTA\n");
            Serial.printf("  h/?  - esta ajuda\n");
            Serial.printf("  Browser: http://%s\n", WiFi.localIP().toString().c_str());
            if (s_bridge_discovered)
                Serial.printf("  Bridge:  http://%s:%d\n", s_bridge_host, s_bridge_port);
            Serial.printf("  IP local: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("  RSSI:     %d dBm\n", WiFi.RSSI());
            Serial.printf("  Up:       %lu s\n", (millis() - s_start_time) / 1000);
            Serial.printf("----------------\n\n");
            break;
        case 's':
        case 'S':
        {
            unsigned long up = (millis() - s_start_time) / 1000;
            Serial.printf("\n--- Status ---\n");
            Serial.printf("  Dispositivo: %s\n", s_device_id);
            Serial.printf("  Nome:        %s\n", s_device_name);
            Serial.printf("  Tipo:        %s\n", get_device_type_string());
            Serial.printf("  Nivel:       %d %%\n", s_level);
            Serial.printf("  Estado:      %s\n", s_on ? "LIGADO" : "DESLIGADO");
            Serial.printf("  PWM:         %d / 1023\n", map(s_level, 0, 100, 0, 1023));
            Serial.printf("  Bateria:     %d %%\n", s_battery);
            Serial.printf("  Bridge:      http://%s:%d (%s)\n", s_bridge_host, s_bridge_port,
                          s_bridge_connected ? "conectado" : "desconectado");
            Serial.printf("  Browser:     http://%s\n", WiFi.localIP().toString().c_str());
            Serial.printf("  RSSI:        %d dBm\n", WiFi.RSSI());
            Serial.printf("  Uptime:      %lu s\n", up);
            Serial.printf("---------------\n\n");
            break;
        }
    }
}

void setup(void)
{
    Serial.begin(115200);
    delay(1000);
    s_start_time = millis();

    uint32_t chip_id = ESP.getChipId();
    snprintf(s_device_id, sizeof(s_device_id), "esp8266_%06x", chip_id);

    load_device_name();

    Serial.printf("\n");
    Serial.printf("============================================\n");
    Serial.printf("  ESP8266 Dimmable Light v1.0\n");
    Serial.printf("  Device : %s\n", s_device_id);
    Serial.printf("  Nome   : %s\n", s_device_name);
    Serial.printf("  Tipo   : %s\n", get_device_type_string());
    Serial.printf("============================================\n");

    randomSeed(analogRead(A0));
    init_hardware();

    WiFi.setAutoReconnect(true);

    s_udp.begin(DISCOVERY_PORT);
    Serial.printf("[%s] UDP listener on port %d\n", TAG, DISCOVERY_PORT);

    if (!wifi_setup(false))
    {
        Serial.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }

    ArduinoOTA.setHostname(s_device_id);
    ArduinoOTA.onStart([]() { Serial.printf("[%s] OTA update start\n", TAG); });
    ArduinoOTA.onEnd([]() { Serial.printf("[%s] OTA update end\n", TAG); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[%s] OTA progress: %u%%\r", TAG, (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[%s] OTA error: %d\n", TAG, error);
    });
    ArduinoOTA.begin();
    Serial.printf("[%s] OTA ready: %s.local\n", TAG, s_device_id);

    s_server.on("/", handle_root);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/set", handle_api_set);
    s_server.begin();
    Serial.printf("\n  => Browser: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  => Terminal: 'h' comando de ajuda\n");

    if (discover_bridge())
    {
        register_device();
        s_pending_register_state = true;
    }
    else
    {
        Serial.printf("[%s] No bridge available yet, waiting for discovery\n", TAG);
    }
    Serial.printf("============================================\n");
    Serial.printf("  Pronto! Pressione 'h' para ajuda\n");
    Serial.printf("============================================\n\n");
}

void loop(void)
{
    handle_serial();
    check_config_portal_timeout();
    ArduinoOTA.handle();
    s_server.handleClient();

    if (WiFi.status() != WL_CONNECTED)
    {
        maintain_wifi_connection();
        delay(1000);
        return;
    }

    maintain_bridge_discovery();
    poll_commands();

    unsigned long now = millis();

    if (!s_bridge_connected && s_bridge_discovered && now - s_last_bridge_reconnect > 30000)
    {
        s_last_bridge_reconnect = now;
        if (register_device())
        {
            s_last_state_update = now;
            s_pending_register_state = true;
        }
    }

    if (s_pending_register_state)
    {
        s_pending_register_state = false;
        send_state(true);
    }

    if (now - s_last_telemetry_update > TELEMETRY_INTERVAL)
    {
        s_last_telemetry_update = now;
        Serial.printf("[%s] RSSI=%d dBm  up=%lus\n", TAG, WiFi.RSSI(), (millis() - s_start_time) / 1000);
        send_heartbeat();
    }

    if (now - s_last_state_update > STATE_UPDATE_INTERVAL)
    {
        s_last_state_update = now;
        send_state(false);
    }

    static unsigned long s_last_force_send = 0;
    if (now - s_last_force_send > STATE_FORCE_INTERVAL)
    {
        s_last_force_send = now;
        send_state(true);
    }

#ifdef LED_PIN
    static unsigned long last_led = 0;
    if (s_wifi_configuration_mode)
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else if (WiFi.status() != WL_CONNECTED)
    {
        if (now - last_led >= LED_BLINK_WIFI_MS)
        {
            last_led = now;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    else if (!s_bridge_discovered)
    {
        if (now - last_led >= LED_BLINK_BRIDGE_MS)
        {
            last_led = now;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
    }
#endif

    delay(1);
}