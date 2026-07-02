#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <DHT.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <espnow.h>
#include "config.h"
#include "pages.h"
#include "espnow_protocol.h"

static const char *TAG = "esp8266-dht-gas";

typedef struct __attribute__((packed)) {
    float temperature;
    float humidity;
    uint16_t gas_level;
    uint8_t alarm;
} payload_dht_gas_t;

static DHT *s_dht = nullptr;

static unsigned long s_last_state_update = 0;
static unsigned long s_last_telemetry_update = 0;
static unsigned long s_last_reconnect_attempt = 0;
static unsigned long s_last_espnow_send = 0;
static unsigned long s_last_espnow_pair = 0;
static unsigned long s_last_heartbeat = 0;

static bool s_gateway_connected = false;
static bool s_paired = false;
static uint8_t s_gateway_mac[6];
static uint16_t s_sequence = 0;
static uint16_t s_assigned_slot = 0;
static int s_pair_attempts = 0;
static bool s_ack_received = false;
static bool s_send_pending = false;
static bool s_espnow_ready = false;

static float s_temperature = 0;
static float s_humidity = 0;
static int s_gas_level = 0;
static bool s_alarm = false;
static bool s_sensor_error = false;
static bool s_dht_valid = false;
static int s_battery = 100;
static unsigned long s_start_time = 0;
static unsigned long s_last_send_ms = 0;

static char s_device_id[32];
static char s_device_name[48] = DEVICE_NAME;

static int s_dht_pin = DHT_PIN;
static bool s_wifi_configuration_mode = false;
static unsigned long s_wifi_config_start_time = 0;

static ESP8266WebServer s_server(80);

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_MAGIC 0xAA

static void save_gateway_mac(const uint8_t *mac)
{
    EEPROM.begin(128);
    EEPROM.write(EEPROM_GATEWAY_MAC_ADDR, EEPROM_MAGIC);
    for (int i = 0; i < 6; i++)
        EEPROM.write(EEPROM_GATEWAY_MAC_ADDR + 1 + i, mac[i]);
    EEPROM.commit();
    EEPROM.end();
}

static bool load_gateway_mac(void)
{
    EEPROM.begin(128);
    uint8_t marker = EEPROM.read(EEPROM_GATEWAY_MAC_ADDR);
    if (marker == EEPROM_MAGIC)
    {
        for (int i = 0; i < 6; i++)
            s_gateway_mac[i] = EEPROM.read(EEPROM_GATEWAY_MAC_ADDR + 1 + i);
        EEPROM.end();
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        Serial.printf("[%s] Loaded gateway MAC: %s\n", TAG, mac_str);
        return true;
    }
    EEPROM.end();
    return false;
}

static void save_device_name(const char *name)
{
    EEPROM.begin(128);
    EEPROM.write(EEPROM_NAME_ADDR, 0xFF);
    for (int i = 0; i < EEPROM_NAME_MAX - 1; i++)
    {
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
    for (int i = 0; s[i]; i++)
    {
        char c = s[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

static void load_device_name(void)
{
    EEPROM.begin(128);
    uint8_t marker = EEPROM.read(EEPROM_NAME_ADDR);
    if (marker == 0xFF)
    {
        char buf[EEPROM_NAME_MAX];
        for (int i = 0; i < EEPROM_NAME_MAX - 1; i++)
        {
            buf[i] = EEPROM.read(EEPROM_NAME_ADDR + 1 + i);
            if (buf[i] == '\0') break;
        }
        buf[EEPROM_NAME_MAX - 1] = '\0';
        if (is_valid_name(buf))
        {
            strncpy(s_device_name, buf, sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
        }
    }
    EEPROM.end();
}

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    (void)status;
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    if (!data || len < 1) return;

    switch (data[0])
    {
        case ESPNOW_MSG_PAIR_RESPONSE:
        {
            if (len < sizeof(espnow_pair_response_t)) return;
            espnow_pair_response_t *resp = (espnow_pair_response_t *)data;
            if (resp->status == PAIR_STATUS_OK)
            {
                mac_copy(s_gateway_mac, mac);
                s_assigned_slot = resp->assigned_slot;
                save_gateway_mac(mac);
                s_paired = true;
                s_gateway_connected = true;
                char mac_str[18];
                mac_to_str(mac, mac_str, sizeof(mac_str));
                Serial.printf("[%s] Paired with gateway %s slot %d\n", TAG, mac_str, s_assigned_slot);
            }
            else
            {
                Serial.printf("[%s] Pair response: status=%d\n", TAG, resp->status);
            }
            break;
        }
        case ESPNOW_MSG_ACK:
        {
            if (len < sizeof(espnow_ack_t)) return;
            espnow_ack_t *ack = (espnow_ack_t *)data;
            if (ack->status == PAIR_STATUS_DENIED)
            {
                s_paired = false;
                s_gateway_connected = false;
                Serial.printf("[%s] Gateway rejected data (denied), need re-pair\n", TAG);
            }
            else
            {
                s_gateway_connected = true;
            }
            s_ack_received = true;
            break;
        }
    }
}

static bool espnow_init_client(void)
{
    if (esp_now_init() != 0)
    {
        Serial.printf("[%s] ESP-NOW init failed\n", TAG);
        return false;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);
    s_espnow_ready = true;
    Serial.printf("[%s] ESP-NOW initialized\n", TAG);
    return true;
}

static bool espnow_add_peer(const uint8_t *mac)
{
    if (!s_espnow_ready) return false;
    esp_now_del_peer((uint8_t *)mac);
    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    int ret = esp_now_add_peer((uint8_t *)mac, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    if (ret != 0)
    {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        Serial.printf("[%s] Failed to add peer %s: %d\n", TAG, mac_str, ret);
    }
    return (ret == 0);
}

#define ESPNOW_HEADER_FIXED_SIZE (sizeof(espnow_header_t) - sizeof(((espnow_header_t*)0)->payload))

static bool espnow_send_data(void)
{
    if (!s_paired || !s_espnow_ready) return false;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_dht_gas_t)];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_SENSOR_DATA;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_DHT_GAS;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();

    payload_dht_gas_t *pl = (payload_dht_gas_t *)hdr->payload;
    pl->temperature = s_temperature;
    pl->humidity = s_humidity;
    pl->gas_level = (uint16_t)s_gas_level;
    pl->alarm = s_alarm ? 1 : 0;
    hdr->payload_len = sizeof(payload_dht_gas_t);

    if (!espnow_add_peer(s_gateway_mac))
    {
        Serial.printf("[%s] Failed to add gateway peer\n", TAG);
        return false;
    }

    s_ack_received = false;
    s_send_pending = true;
    int ret = esp_now_send(s_gateway_mac, buf, sizeof(buf));
    if (ret != 0)
    {
        Serial.printf("[%s] ESP-NOW send failed: %d\n", TAG, ret);
        s_send_pending = false;
        return false;
    }
    return true;
}

static bool espnow_send_heartbeat(void)
{
    if (!s_paired || !s_espnow_ready) return false;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_HEARTBEAT;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_DHT_GAS;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();
    hdr->payload_len = 0;

    if (!espnow_add_peer(s_gateway_mac)) return false;

    s_ack_received = false;
    return esp_now_send(s_gateway_mac, buf, sizeof(buf)) == 0;
}

static bool espnow_send_pair_request(void)
{
    if (!s_espnow_ready) return false;

    uint8_t buf[sizeof(espnow_pair_request_t)];
    memset(buf, 0, sizeof(buf));

    espnow_pair_request_t *req = (espnow_pair_request_t *)buf;
    req->msg_type = ESPNOW_MSG_PAIR_REQUEST;
    req->sequence = s_sequence++;
    WiFi.macAddress(req->sensor_mac);
    req->sensor_type = SENSOR_TYPE_DHT_GAS;
    uint32_t ver = 0x000A000B;
    req->firmware_version[0] = (uint8_t)(ver >> 24);
    req->firmware_version[1] = (uint8_t)(ver >> 16);
    req->firmware_version[2] = (uint8_t)(ver >> 8);
    req->firmware_version[3] = (uint8_t)(ver);
    strncpy(req->device_name, s_device_name, sizeof(req->device_name) - 1);
    req->device_name[sizeof(req->device_name) - 1] = '\0';

    if (!espnow_add_peer(s_broadcast_mac)) return false;

    s_ack_received = false;
    int ret = esp_now_send(s_broadcast_mac, buf, sizeof(buf));
    if (ret != 0)
    {
        Serial.printf("[%s] Pair request send failed: %d\n", TAG, ret);
        return false;
    }
    return true;
}

static void read_sensors(void)
{
    float temp = s_dht->readTemperature();
    float hum = s_dht->readHumidity();
    if (!isnan(temp) && !isnan(hum))
    {
        s_temperature = temp;
        s_humidity = hum;
        s_dht_valid = true;
    }
    else
    {
        s_dht_valid = false;
        s_temperature += (random(-10, 10) / 20.0);
        s_humidity += (random(-20, 20) / 10.0);
        if (s_temperature < 18.0) s_temperature = 18.0;
        if (s_temperature > 30.0) s_temperature = 30.0;
        if (s_humidity < 30.0) s_humidity = 30.0;
        if (s_humidity > 70.0) s_humidity = 70.0;
    }

    int raw = analogRead(GAS_ANALOG_PIN);
    s_sensor_error = (raw == 0 || raw == 1024);
    s_gas_level = constrain(map(raw, 0, 1024, 0, 100), 0, 100);

    bool alarm = (s_gas_level >= GAS_ALARM_THRESHOLD);
    bool alert = (s_gas_level >= GAS_ALERT_THRESHOLD && s_gas_level < GAS_ALARM_THRESHOLD);
    s_alarm = alarm || alert;

    static int counter = 0;
    counter++;
    if (counter > 100)
    {
        counter = 0;
        s_battery = max(0, s_battery - 1);
    }
}

static int detect_dht_pin(void)
{
    const int candidates[] = {DHT_PIN, 4, 12, 13, 14, 0, 2, 15};
    Serial.printf("[%s] Detectando pino do DHT22...\n", TAG);
    for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++)
    {
        int pin = candidates[i];
        pinMode(pin, INPUT_PULLUP);
        DHT dht(pin, DHT_TYPE);
        dht.begin();
        delay(250);
        float t = dht.readTemperature();
        float h = dht.readHumidity();
        if (!isnan(t) && !isnan(h))
        {
            Serial.printf("[%s] DHT22 encontrado no GPIO %d (T=%.1f H=%.1f)\n", TAG, pin, t, h);
            return pin;
        }
    }
    Serial.printf("[%s] DHT22 nao encontrado, usando GPIO %d\n", TAG, DHT_PIN);
    return DHT_PIN;
}

static void init_hardware(void)
{
    s_dht_pin = detect_dht_pin();
    s_dht = new DHT(s_dht_pin, DHT_TYPE);
    s_dht->begin();

    pinMode(GAS_ANALOG_PIN, INPUT);

    pinMode(GAS_LED_ALERT_PIN, OUTPUT);
    digitalWrite(GAS_LED_ALERT_PIN, LOW);

    pinMode(GAS_LED_ALARM_PIN, OUTPUT);
    digitalWrite(GAS_LED_ALARM_PIN, LOW);

#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif
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

    WiFiManagerParameter custom_dev_name("dev_name", "Device Name", s_device_name, 48);
    wifiManager.addParameter(&custom_dev_name);

    if (wifiManager.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
    {
        if (strlen(custom_dev_name.getValue()) > 0 && strcmp(s_device_name, custom_dev_name.getValue()) != 0)
        {
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
        if (s_dht_valid) {
            doc["temperature"] = s_temperature;
            doc["humidity"] = s_humidity;
        }
        doc["gas_level"] = s_gas_level;
        doc["alarm"] = s_alarm;
        doc["battery"] = s_battery;
        doc["dht_pin"] = s_dht_pin;
        doc["dht_valid"] = s_dht_valid;
        doc["device_id"] = s_device_id;
        doc["device_name"] = s_device_name;
        doc["gateway_connected"] = s_gateway_connected;
        doc["paired"] = s_paired;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        if (s_last_send_ms) doc["last_send_s"] = (millis() - s_last_send_ms) / 1000;
        doc["gas_analog_pin"] = GAS_ANALOG_PIN;
        doc["gas_digital_pin"] = GAS_DIGITAL_PIN;
        doc["led_pin"] = LED_PIN;
        doc["gas_led_alert_pin"] = GAS_LED_ALERT_PIN;
        doc["gas_led_alarm_pin"] = GAS_LED_ALARM_PIN;
        doc["raw_analog"] = analogRead(GAS_ANALOG_PIN);
        doc["gas_digital"] = digitalRead(GAS_DIGITAL_PIN);
        doc["led_state"] = digitalRead(LED_PIN);
        doc["alert_led_state"] = digitalRead(GAS_LED_ALERT_PIN);
        doc["alarm_led_state"] = digitalRead(GAS_LED_ALARM_PIN);
        doc["slot"] = s_assigned_slot;
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}

static void handle_api_pin(void)
{
    if (s_server.method() == HTTP_GET)
    {
        int pin = s_server.arg("gpio").toInt();
        pinMode(pin, INPUT_PULLUP);
        int state = digitalRead(pin);
        String json;
        JsonDocument doc;
        doc["gpio"] = pin;
        doc["state"] = state;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    }
    else if (s_server.method() == HTTP_POST)
    {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err)
        {
            s_server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        int pin = doc["gpio"] | -1;
        if (pin < 0 || pin > 16)
        {
            s_server.send(400, "application/json", "{\"error\":\"invalid gpio\"}");
            return;
        }
        int state = doc["state"] | -1;
        if (state != 0 && state != 1)
        {
            s_server.send(400, "application/json", "{\"error\":\"state must be 0 or 1\"}");
            return;
        }
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state);
        String json;
        JsonDocument resp;
        resp["gpio"] = pin;
        resp["state"] = state;
        resp["status"] = "ok";
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
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
    {
        Serial.printf("\n--- Leitura forcada ---\n");
        read_sensors();
        Serial.printf("  Temperatura: %.1f C%s\n", s_temperature, s_dht_valid ? "" : " (invalido)");
        Serial.printf("  Umidade:     %.1f %%\n", s_humidity);
        Serial.printf("  Gas:         %d %%\n", s_gas_level);
        Serial.printf("  Alarme:      %s\n", s_alarm ? "SIM - VAZAMENTO!" : "Seguro");
        Serial.printf("  DHT22:       GPIO %d\n", s_dht_pin);
        Serial.printf("  Bateria:     %d %%\n", s_battery);
        if (s_paired)
        {
            s_last_espnow_send = 0;
            if (espnow_send_data())
                s_last_send_ms = millis();
        }
        else
        {
            Serial.printf("  (gateway nao pareado)\n");
        }
        Serial.printf("-----------------------------\n\n");
        break;
    }
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
    case 't':
    case 'T':
    {
        Serial.printf("\n--- Teste de pinos DHT22 ---\n");
        int pin = detect_dht_pin();
        if (pin >= 0)
            Serial.printf("  => DHT22 no GPIO %d\n", pin);
        else
            Serial.printf("  => DHT22 nao encontrado\n");
        Serial.printf("----------------------------\n\n");
        break;
    }
    case 'p':
    case 'P':
    {
        Serial.printf("\n--- Par ---\n");
        s_paired = false;
        s_gateway_connected = false;
        s_pair_attempts = 0;
        Serial.printf("  Estado de pareamento resetado\n");
        Serial.printf("  Enviando requisição de par...\n");
        if (espnow_send_pair_request())
            Serial.printf("  Requisição enviada!\n");
        else
            Serial.printf("  Falha ao enviar requisição\n");
        Serial.printf("----------------\n\n");
        break;
    }
    case 'h':
    case 'H':
    case '?':
        Serial.printf("\n--- Comandos ---\n");
        Serial.printf("  l    - ler sensores agora\n");
        Serial.printf("  r    - reset\n");
        Serial.printf("  s    - status do dispositivo\n");
        Serial.printf("  t    - testar pinos do DHT22\n");
        Serial.printf("  p    - resetar par e tentar parear\n");
        Serial.printf("  u    - info OTA\n");
        Serial.printf("  h/?  - esta ajuda\n");
        Serial.printf("  Browser: http://%s\n", WiFi.localIP().toString().c_str());
        if (s_paired)
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            Serial.printf("  Gateway: %s (slot %d)\n", mac_str, s_assigned_slot);
        }
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
        Serial.printf("  Temperatura: %.1f C%s\n", s_temperature, s_dht_valid ? "" : " (invalido)");
        Serial.printf("  Umidade:     %.1f %%\n", s_humidity);
        Serial.printf("  Gas:         %d %%\n", s_gas_level);
        Serial.printf("  Alarme:      %s\n", s_alarm ? "SIM" : "nao");
        Serial.printf("  Bateria:     %d %%\n", s_battery);
        if (s_paired)
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            Serial.printf("  Gateway:     %s (slot %d) %s\n", mac_str, s_assigned_slot,
                          s_gateway_connected ? "conectado" : "desconectado");
        }
        else
        {
            Serial.printf("  Gateway:     nao pareado\n");
        }
        Serial.printf("  Browser:     http://%s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  RSSI:        %d dBm\n", WiFi.RSSI());
        Serial.printf("  Uptime:      %lu s\n", up);
        Serial.printf("---------------\n\n");
        break;
    }
    }
}

static void handle_ota(void)
{
    if (!Update.hasError())
    {
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(500);
        ESP.restart();
    }
    else
    {
        s_server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

static void handle_ota_upload(void)
{
    HTTPUpload &upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("[%s] OTA update started: %s (%d bytes)\n", TAG, upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(upload.totalSize))
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
            Serial.printf("[%s] OTA update success: %d bytes\n", TAG, upload.totalSize);
        else
            Update.printError(Serial);
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
    Serial.printf("  ESP8266 DHT22 + MQ-2 " FW_VERSION "\n");
    Serial.printf("  Device: %s\n", s_device_id);
    Serial.printf("  Nome:   %s\n", s_device_name);
    Serial.printf("============================================\n");

    randomSeed(analogRead(A0));
    init_hardware();
    Serial.printf("============================================\n");

    if (!wifi_setup(false))
    {
        Serial.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }

    espnow_init_client();

    s_server.on("/", handle_root);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/pin", handle_api_pin);
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
    s_server.begin();

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

    Serial.printf("\n  => Browser: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  => Terminal: 'h' comando de ajuda\n");

    if (load_gateway_mac())
    {
        Serial.printf("[%s] Gateway MAC loaded from EEPROM\n", TAG);
        s_paired = true;
    }
    else
    {
        Serial.printf("[%s] No saved gateway MAC, will pair\n", TAG);
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

    unsigned long now = millis();

    static unsigned long s_last_sensor_read = 0;
    if (now - s_last_sensor_read > STATE_UPDATE_INTERVAL)
    {
        s_last_sensor_read = now;
        read_sensors();
    }

    if (!s_paired)
    {
        if (now - s_last_espnow_pair > ESPNOW_PAIR_INTERVAL_MS)
        {
            s_last_espnow_pair = now;
            s_pair_attempts++;
            Serial.printf("[%s] Pair attempt %d/%d\n", TAG, s_pair_attempts, ESPNOW_MAX_PAIR_ATTEMPTS);
            espnow_send_pair_request();
            if (s_pair_attempts >= ESPNOW_MAX_PAIR_ATTEMPTS)
            {
                s_pair_attempts = 0;
                Serial.printf("[%s] Max pair attempts, waiting before retry\n", TAG);
                delay(60000);
            }
        }
        delay(1);
        return;
    }

    if (now - s_last_espnow_send > STATE_UPDATE_INTERVAL)
    {
        s_last_espnow_send = now;
        s_ack_received = false;
        if (espnow_send_data())
        {
            unsigned long deadline = millis() + ESPNOW_ACK_TIMEOUT_MS;
            while (millis() < deadline)
            {
                if (s_ack_received) break;
                delay(1);
            }
            if (s_ack_received)
            {
                s_gateway_connected = true;
                s_last_send_ms = millis();
            }
            else
            {
                s_gateway_connected = false;
                bool ok = false;
                for (int retry = 0; retry < ESPNOW_SEND_RETRIES; retry++)
                {
                    delay(50);
                    s_ack_received = false;
                    if (espnow_send_data())
                    {
                        deadline = millis() + ESPNOW_ACK_TIMEOUT_MS;
                        while (millis() < deadline)
                        {
                            if (s_ack_received) break;
                            delay(1);
                        }
                        if (s_ack_received) { ok = true; break; }
                    }
                }
                if (ok)
                {
                    s_gateway_connected = true;
                    s_last_send_ms = millis();
                }
                else
                {
                    Serial.printf("[%s] Send failed after %d retries, re-pairing\n", TAG, ESPNOW_SEND_RETRIES);
                    s_paired = false;
                    s_pair_attempts = 0;
                    s_last_espnow_pair = 0;
                }
            }
        }
    }

    if (now - s_last_heartbeat > HEARTBEAT_INTERVAL)
    {
        s_last_heartbeat = now;
        Serial.printf("[%s] RSSI=%d dBm  up=%lus\n", TAG, WiFi.RSSI(), (millis() - s_start_time) / 1000);
        if (s_paired)
            espnow_send_heartbeat();
    }

    // LED status
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
    else if (!s_paired)
    {
        if (now - last_led >= LED_BLINK_GATEWAY_MS)
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

    // Gas alert LED
    #ifdef GAS_LED_ALERT_PIN
    if (!s_sensor_error && s_gas_level < GAS_ALERT_THRESHOLD)
    {
        digitalWrite(GAS_LED_ALERT_PIN, LOW);
    }
    else if (s_gas_level >= GAS_ALERT_THRESHOLD && s_gas_level < GAS_ALARM_THRESHOLD)
    {
        static unsigned long last_alert = 0;
        if (now - last_alert >= ALARM_BLINK_SLOW_MS)
        {
            last_alert = now;
            digitalWrite(GAS_LED_ALERT_PIN, !digitalRead(GAS_LED_ALERT_PIN));
        }
    }
    else if (s_gas_level >= GAS_ALARM_THRESHOLD)
    {
        digitalWrite(GAS_LED_ALERT_PIN, HIGH);
    }
    #endif

    // Gas alarm LED
    #ifdef GAS_LED_ALARM_PIN
    static unsigned long last_alarm_led = 0;
    if (s_gas_level >= GAS_ALARM_THRESHOLD)
    {
        if (now - last_alarm_led >= ALARM_BLINK_FAST_MS)
        {
            last_alarm_led = now;
            digitalWrite(GAS_LED_ALARM_PIN, !digitalRead(GAS_LED_ALARM_PIN));
        }
    }
    else
    {
        digitalWrite(GAS_LED_ALARM_PIN, LOW);
    }
    #endif

    delay(1);
}
