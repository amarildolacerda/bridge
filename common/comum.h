#ifndef COMUM_H
#define COMUM_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#ifndef FW_VERSION
#define FW_VERSION "v1.1.0"
#endif

#ifndef DEVICE_ID_PREFIX
#define DEVICE_ID_PREFIX "esp8266_"
#endif

#ifndef BRIDGE_HOST
#define BRIDGE_HOST "0.0.0.0"
#endif

#ifndef BRIDGE_PORT
#define BRIDGE_PORT 80
#endif

#ifndef WIFI_TIMEOUT_RESET_MS
#define WIFI_TIMEOUT_RESET_MS 120000
#endif

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 512
#endif

#ifndef MAGIC_NUMBER
#define MAGIC_NUMBER 0x42
#endif

#ifndef MAGIC_NUMBER_ADDR
#define MAGIC_NUMBER_ADDR 0
#endif

#ifndef PIN_CONFIG_ADDR
#define PIN_CONFIG_ADDR 1
#endif

#ifndef NOME_CONFIG_ADDR
#define NOME_CONFIG_ADDR 2
#endif

#ifndef TIMEOUT_CONFIG_ADDR
#define TIMEOUT_CONFIG_ADDR 34
#endif

// =============================================
// COMMON NAMESPACE
// =============================================
namespace Comum
{
    // WiFi watchdog variables
    bool wasWiFiConnected = false;
    unsigned long wifiDisconnectedStartTime = 0;
    unsigned long lastWiFiCheck = 0;

    // Device identification
    String deviceId;
    String deviceName;
    String mdnsName;
    bool mdnsStarted = false;

    // WebSocket server (port 81)
    WebSocketsServer webSocket(81);

    // =============================================
    // WIFI WATCHDOG WITH AUTO RESET
    // =============================================
    void verificarWiFi()
    {
        bool isConnected = (WiFi.status() == WL_CONNECTED);

        if (isConnected != wasWiFiConnected)
        {
            if (isConnected)
            {
                Serial.println("[WiFi] Conectado/Reconectado!");
                wifiDisconnectedStartTime = 0;
                if (!mdnsStarted)
                {
                    iniciarMDNS();
                }
            }
            else
            {
                Serial.println("[WiFi] Desconectado! Iniciando contagem para reset...");
                wifiDisconnectedStartTime = millis();
                mdnsStarted = false;
            }
            wasWiFiConnected = isConnected;
        }

        if (!isConnected && wifiDisconnectedStartTime > 0)
        {
            unsigned long disconnectedDuration = millis() - wifiDisconnectedStartTime;

            if (disconnectedDuration >= WIFI_TIMEOUT_RESET_MS)
            {
                Serial.println("\n========================================");
                Serial.print("⚠️  SEM CONEXÃO WI-FI POR ");
                Serial.print(disconnectedDuration / 1000);
                Serial.println(" SEGUNDOS!");
                Serial.println("🔄 EXECUTANDO RESET AUTOMÁTICO...");
                Serial.println("========================================\n");

                for (int i = 0; i < 5; i++)
                {
                    digitalWrite(LED_BUILTIN, LOW);
                    delay(150);
                    digitalWrite(LED_BUILTIN, HIGH);
                    delay(150);
                }

                delay(1000);
                ESP.restart();
            }
            else if (disconnectedDuration >= 30000)
            {
                if ((disconnectedDuration / 1000) % 10 == 0)
                {
                    Serial.print("[WiFi] Ainda desconectado. Reset em ");
                    Serial.print((WIFI_TIMEOUT_RESET_MS - disconnectedDuration) / 1000);
                    Serial.println(" segundos...");
                }

                if ((millis() % 500) < 250)
                {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                else
                {
                    digitalWrite(LED_BUILTIN, HIGH);
                }
            }
        }

        if (!isConnected && millis() - lastWiFiCheck > 30000)
        {
            Serial.println("[WiFi] Tentando reconectar...");
            WiFi.reconnect();
            lastWiFiCheck = millis();
        }
    }

    // =============================================
    // DEVICE NAMING (MAC-based unique names)
    // =============================================
    String gerarDeviceId()
    {
        {
        {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        return String(DEVICE_ID_PREFIX) + mac;
    }

    String gerarNomeMDNS(String nome)
    {
        String nome_mdns = nome;
        nome_mdns.toLowerCase();

        for (int i = 0; i < nome_mdns.length(); i++)
        {
            char c = nome_mdns[i];
            if (!(isalnum(c) || c == '-'))
            {
                nome_mdns.setCharAt(i, '_');
            }
        }

        if (nome_mdns.length() > 0 && !isalpha(nome_mdns[0]))
        {
            nome_mdns = "device_" + nome_mdns;
        }

        if (nome_mdns.length() > 63)
        {
            nome_mdns = nome_mdns.substring(0, 63);
        }

        return nome_mdns;
    }

    // =============================================
    // MDNS SETUP
    // =============================================
    void iniciarMDNS()
    {
        if (mdnsStarted)
        {
            MDNS.end();
            delay(100);
        }

        mdnsName = gerarNomeMDNS(deviceName);
        deviceId = gerarDeviceId();

        Serial.println("\n=== INICIANDO mDNS ===");
        Serial.print("Device ID: ");
        Serial.println(deviceId);
        Serial.print("Nome mDNS: ");
        Serial.print(mdnsName);
        Serial.println(".local");

        if (MDNS.begin(mdnsName.c_str()))
        {
            MDNS.addService("http", "tcp", BRIDGE_PORT);
            MDNS.addService("ws", "tcp", 81);
            MDNS.addService("esp-bridge", "udp", 5000);

            Serial.println("mDNS iniciado com sucesso!");
            Serial.print("Acesse: http://");
            Serial.print(mdnsName);
            Serial.println(".local");
            Serial.print("WebSocket: ws://");
            Serial.print(mdnsName);
            Serial.println(".local:81");

            mdnsStarted = true;
        }
        else
        {
            Serial.println("ERRO: Falha ao iniciar mDNS!");
            mdnsStarted = false;
        }
        Serial.println("========================\n");
    }

    // =============================================
    // EEPROM CONFIG HELPERS
    // =============================================
    bool carregarConfigBasica(int &pinOut, String &nameOut, int &timeoutOut)
    {
        EEPROM.begin(EEPROM_SIZE);

        int magic = EEPROM.read(MAGIC_NUMBER_ADDR);
        if (magic != MAGIC_NUMBER)
        {
            EEPROM.end();
            return false;
        }

        pinOut = EEPROM.read(PIN_CONFIG_ADDR);

        char nome_buffer[33] = {0};
        int i = 0;
        while (i < 32)
        {
            char c = EEPROM.read(NOME_CONFIG_ADDR + i);
            if (c == '\0')
                break;
            nome_buffer[i] = c;
            i++;
        }
        nome_buffer[i] = '\0';
        nameOut = String(nome_buffer);

        timeoutOut = EEPROM.read(TIMEOUT_CONFIG_ADDR);

        EEPROM.end();
        return true;
    }

    void salvarConfigBasica(int pin, String name, int timeout)
    {
        EEPROM.begin(EEPROM_SIZE);
        EEPROM.write(MAGIC_NUMBER_ADDR, MAGIC_NUMBER);
        EEPROM.write(PIN_CONFIG_ADDR, pin);

        for (int i = 0; i < name.length() && i < 32; i++)
        {
            EEPROM.write(NOME_CONFIG_ADDR + i, name[i]);
        }
        EEPROM.write(NOME_CONFIG_ADDR + name.length(), '\0');

        EEPROM.write(TIMEOUT_CONFIG_ADDR, timeout);
        EEPROM.commit();
        EEPROM.end();
    }

    void limparEEPROM()
    {
        EEPROM.begin(EEPROM_SIZE);
        for (int i = 0; i < EEPROM_SIZE; i++)
        {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
        EEPROM.end();
    }

    // =============================================
    // WIFI MANAGER PORTAL
    // =============================================
    void iniciarPortalConfiguracao()
    {
        WiFiManager wifiManager;
        wifiManager.setConfigPortalTimeout(180);
        wifiManager.setConnectTimeout(30);

        String apName = "PROV_" + gerarDeviceId();
        Serial.println("\n=== PORTAL DE CONFIGURAÇÃO ===");
        Serial.print("Conecte-se à rede: ");
        Serial.println(apName);
        Serial.print("Acesse: http://192.168.4.1");
        Serial.println("==============================\n");

        if (!wifiManager.autoConnect(apName.c_str()))
        {
            Serial.println("Falha no portal, reiniciando...");
            delay(3000);
            ESP.restart();
        }

        Serial.println("WiFi configurado com sucesso!");
    }

    // =============================================
    // WEBSOCKET EVENT HANDLER
    // =============================================
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
    {
        switch (type)
        {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Cliente %u desconectado\n", num);
            break;

        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[WS] Cliente %u conectado de %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                String status = "{\"status\":\"connected\",\"device\":\"" + deviceName + "\",\"id\":\"" + deviceId + "\"}";
                webSocket.sendTXT(num, status);
                enviarInfoDispositivo(num);
            }
            break;

        case WStype_TEXT:
            {
                String message = String((char *)payload);
                Serial.printf("[WS] Recebido do cliente %u: %s\n", num, message.c_str());

                if (message == "restart")
                {
                    webSocket.sendTXT(num, "{\"response\":\"restarting\"}");
                    delay(100);
                    ESP.restart();
                }
                else if (message == "status")
                {
                    enviarInfoDispositivo(num);
                }
                else if (message == "toggle")
                {
                    // Override in client app
                }
                else if (message.startsWith("config:"))
                {
                    // Handle config commands
                }
            }
            break;

        case WStype_BIN:
            Serial.printf("[WS] Mensagem binária do cliente %u\n", num);
            break;
        }
    }

    void enviarInfoDispositivo(uint8_t clientNum = 255)
    {
        String info = "{";
        info += "\"device\":\"" + deviceName + "\",";
        info += "\"id\":\"" + deviceId + "\",";
        info += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        info += "\"mac\":\"" + WiFi.macAddress() + "\",";
        info += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        info += "\"fw_version\":\"" + String(FW_VERSION) + "\",";
        info += "\"uptime\":" + String(millis() / 1000) + ",";
        info += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        info += "\"mdns\":" + String(mdnsStarted ? "true" : "false");
        info += "}";

        if (clientNum == 255)
        {
            webSocket.broadcastTXT(info);
            Serial.println("[WS] Info broadcast: " + info);
        }
        else
        {
            webSocket.sendTXT(clientNum, info);
            Serial.println("[WS] Info para cliente " + String(clientNum) + ": " + info);
        }
    }

    void webSocketLoop()
    {
        webSocket.loop();
    }

    void webSocketBegin()
    {
        webSocket.begin();
        webSocket.onEvent(webSocketEvent);
        Serial.println("WebSocket server iniciado na porta 81");
    }

    // =============================================
    // LED STATUS INDICATOR
    // =============================================
    void indicarStatus(bool portalAtivo = false)
    {
        static unsigned long lastBlink = 0;
        static bool ledState = false;

        if (portalAtivo)
        {
            if (millis() - lastBlink > 200)
            {
                ledState = !ledState;
                lastBlink = millis();
                digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
            }
        }
        else if (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - lastBlink > 1000)
            {
                ledState = !ledState;
                lastBlink = millis();
                digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
            }
        }
        else
        {
            digitalWrite(LED_BUILTIN, HIGH); // Solid on = connected
        }
    }

    // =============================================
    // SERIAL COMMAND PROCESSOR
    // =============================================
    void processarComandosSerial()
    {
    #ifdef ENABLED_SERIAL
        if (Serial.available())
        {
            String comando = Serial.readStringUntil('\n');
            comando.trim();
            comando.toUpperCase();

            if (comando == "R")
            {
                Serial.println("\n>>> RESETANDO CONFIGURACOES... <<<\n");
                limparEEPROM();
                WiFiManager wifiManager;
                wifiManager.resetSettings();
                delay(1000);
                ESP.restart();
            }
            else if (comando == "CONFIG" || comando == "C")
            {
                Serial.println("\n>>> Abrindo portal de configuracao... <<<\n");
                iniciarPortalConfiguracao();
            }
            else if (comando == "STATUS" || comando == "S")
            {
                Serial.println("\n========================================");
                Serial.println("           STATUS ATUAL");
                Serial.println("========================================");
                Serial.print("FW Version: ");
                Serial.println(FW_VERSION);
                Serial.print("Device ID: ");
                Serial.println(deviceId);
                Serial.print("Device Name: ");
                Serial.println(deviceName);
                Serial.print("WiFi: ");
                Serial.println(WiFi.SSID());
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());
                Serial.print("RSSI: ");
                Serial.print(WiFi.RSSI());
                Serial.println(" dBm");
                Serial.print("Free Heap: ");
                Serial.println(ESP.getFreeHeap());
                Serial.print("Uptime: ");
                Serial.print(millis() / 1000);
                Serial.println(" s");
                Serial.print("mDNS: ");
                Serial.println(mdnsStarted ? "ATIVO" : "INATIVO");
                if (mdnsStarted)
                {
                    Serial.print("   http://");
                    Serial.print(mdnsName);
                    Serial.println(".local");
                }
                Serial.println("========================================\n");
            }
            else if (comando == "HELP" || comando == "H")
            {
                Serial.println("\nCOMANDOS: R=Reset, C=Config, S=Status, H=Help");
            }
        }
    #endif
    }

    // =============================================
    // BOOT BANNER
    // =============================================
    void mostrarBanner(String appName)
    {
        Serial.println("\n");
        Serial.println("========================================");
        Serial.print("ESP8266 ");
        Serial.print(appName);
        Serial.println(" v" FW_VERSION);
        Serial.println("========================================");
        Serial.println("Funcionalidades:");
        Serial.println("- WiFiManager + mDNS");
        Serial.println("- WebSocket (porta 81)");
        Serial.println("- REST API (porta 80)");
        Serial.println("- EEPROM config persistente");
        Serial.println("- Watchdog WiFi com auto-reset");
        Serial.println("- Bridge discovery UDP (porta 5000)");
        Serial.println("========================================\n");
    }

    // =============================================
    // UDP DISCOVERY RESPONSE (for bridge discovery)
    // =============================================
    void handleUDPDiscovery()
    {
        // To be implemented in client app if needed
        // Bridge sends "esp-bridge" broadcast on port 5000
        // Client responds with device info
    }
}

#endif // COMUM_H