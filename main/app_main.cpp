#include <stdio.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>

#include <esp_rmaker_core.h>

#include <app_network.h>

#include "app_device_registry.h"
#include "app_rmaker_gateway.h"
#include "app_wifi_server.h"
#include "app_console.h"

static const char *TAG = "app_main";

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_INIT_DONE:
                ESP_LOGI(TAG, "RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                ESP_LOGI(TAG, "RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(TAG, "RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                ESP_LOGI(TAG, "RainMaker Claim Failed.");
                break;
            default:
                break;
        }
    } else if (event_base == APP_NETWORK_EVENT) {
        switch (event_id) {
            case APP_NETWORK_EVENT_QR_DISPLAY:
                ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
                break;
            case APP_NETWORK_EVENT_PROV_TIMEOUT:
                ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
                break;
            default:
                break;
        }
    }
}

extern "C" void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(device_registry_init());

    app_network_init();

    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(rmaker_gateway_init());
    esp_rmaker_start();

    ESP_ERROR_CHECK(app_network_start(POP_TYPE_NONE));

    int loaded = device_registry_get_loaded_count();
    if (loaded > 0) {
        ESP_LOGI(TAG, "Restoring %d devices from NVS...", loaded);
        int count = 0;
        bridged_device_t *devices = device_registry_get_all(&count);
        for (int i = 0; i < count; i++) {
            if (devices[i].registered && !devices[i].rmaker_device_hdl) {
                esp_err_t err = rmaker_gateway_device_add(devices[i].id, devices[i].type);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Restored RainMaker device: %s (%s)", devices[i].name, devices[i].id);
                } else {
                    ESP_LOGW(TAG, "Failed to restore device %s: %s", devices[i].id, esp_err_to_name(err));
                }
            }
        }
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            wifi_server_update_ip(ip_str);
        }
    }

    ESP_ERROR_CHECK(wifi_server_start());

    ESP_ERROR_CHECK(console_init());

    ESP_LOGI(TAG, "ESP RainMaker Gateway started");
}
