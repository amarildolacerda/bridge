#include "app_console.h"
#include "app_device_registry.h"
#include "app_wifi_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <inttypes.h>

static const char *TAG = "console";
static TaskHandle_t s_console_task = NULL;

static void print_help(void)
{
    printf("\n--- Comandos ---\n");
    printf("  l      - listar devices registrados\n");
    printf("  s      - status geral\n");
    printf("  d      - detalhes de um device\n");
    printf("  b      - broadcast discovery\n");
    printf("  r      - restart\n");
    printf("  h/?    - esta ajuda\n");
}

static void cmd_list(void)
{
    int count = 0;
    bridged_device_t *devices = device_registry_get_all(&count);

    printf("\n--- Devices (%d/%d) ---\n", count, MAX_BRIDGED_DEVICES);
    if (count == 0) {
        printf("  Nenhum device registrado.\n");
        return;
    }

    for (int i = 0; i < MAX_BRIDGED_DEVICES; i++) {
        if (!devices[i].registered) continue;
        const char *online_str = devices[i].online ? "ONLINE" : "OFFLINE";
        int64_t elapsed = esp_timer_get_time() - devices[i].last_seen_us;
        printf("  [%d] %s (%s)\n"
               "       Tipo: %s  IP: %s  Status: %s\n"
               "       State: %s  Último visto: %llds\n",
               i, devices[i].name, devices[i].id,
               device_type_to_string(devices[i].type),
               devices[i].ip, online_str,
               devices[i].state_json[0] ? devices[i].state_json : "{}",
               elapsed / 1000000LL);
    }
}

static void cmd_status(void)
{
    int count = 0;
    device_registry_get_all(&count);

    int64_t uptime_us = esp_timer_get_time();
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    char ip_str[16] = "0.0.0.0";
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        }
    }

    printf("\n--- Status ---\n");
    printf("  IP:        %s\n", ip_str);
    printf("  Devices:   %d registrados\n", count);
    printf("  Uptime:    %llds\n", uptime_us / 1000000LL);
    printf("  Heap:      %" PRIu32 " KB livre (min %" PRIu32 " KB)\n",
           free_heap / 1024, min_free_heap / 1024);
}

static void cmd_detail(void)
{
    char id[64];
    int pos = 0;

    printf("\n  Device ID: ");
    fflush(stdout);

    while (1) {
        int c = getchar();
        if (c == EOF) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (c == '\n' || c == '\r') {
            id[pos] = '\0';
            break;
        }
        if (pos < (int)sizeof(id) - 1) {
            putchar(c);
            id[pos++] = c;
        }
    }

    if (pos == 0) {
        printf("  Cancelado.\n");
        return;
    }

    // Check if input is a numeric index from 'l' output
    bridged_device_t *dev = NULL;
    char *end = NULL;
    long idx = strtol(id, &end, 10);
    if (*end == '\0' && end != id) {
        int count = 0;
        bridged_device_t *all = device_registry_get_all(&count);
        if (idx >= 0 && idx < MAX_BRIDGED_DEVICES && all[idx].registered) {
            dev = &all[idx];
        }
    }
    if (!dev) {
        dev = device_registry_get_by_id(id);
    }
    if (!dev) {
        printf("  Device '%s' nao encontrado.\n", id);
        return;
    }

    int64_t elapsed = esp_timer_get_time() - dev->last_seen_us;
    const char *online_str = dev->online ? "ONLINE" : "OFFLINE";

    printf("\n--- Device: %s ---\n", dev->id);
    printf("  Nome:      %s\n", dev->name);
    printf("  Tipo:      %s\n", device_type_to_string(dev->type));
    printf("  IP:        %s\n", dev->ip);
    printf("  Status:    %s\n", online_str);
    printf("  Último:    %llds atras\n", elapsed / 1000000LL);
    printf("  State:     %s\n", dev->state_json[0] ? dev->state_json : "{}");

    if (dev->pending_command_count > 0) {
        printf("  Commands:  %d pendentes\n", dev->pending_command_count);
        for (int i = 0; i < dev->pending_command_count; i++) {
            printf("    [%d] %s/%s (%s)\n",
                   i, dev->pending_commands[i].cluster,
                   dev->pending_commands[i].command,
                   dev->pending_commands[i].data);
        }
    }
}

static void cmd_broadcast(void)
{
    printf("\n--- Broadcast ---\n");
    wifi_server_broadcast();
    printf("  Bridge announcement enviado.\n");
    printf("  Aguardando 3s por respostas...\n");
    vTaskDelay(pdMS_TO_TICKS(3000));

    discovered_ip_t discovered[MAX_DISCOVERED_IPS];
    int count = wifi_server_get_discovered_ips(discovered, MAX_DISCOVERED_IPS);

    if (count > 0) {
        printf("  Clients descobertos via UDP:\n");
        for (int i = 0; i < count; i++) {
            int64_t ago = (esp_timer_get_time() - discovered[i].last_seen_us) / 1000000LL;
            printf("    [%d] %s @ %s (visto ha %llds)\n",
                   i, discovered[i].id, discovered[i].ip, ago);
        }
    } else {
        printf("  Nenhum client descoberto via UDP.\n");
    }

    int reg_count = 0;
    bridged_device_t *devices = device_registry_get_all(&reg_count);
    if (reg_count > 0) {
        printf("  Devices registrados:\n");
        for (int i = 0; i < MAX_BRIDGED_DEVICES; i++) {
            if (!devices[i].registered) continue;
            const char *online_str = devices[i].online ? "ONLINE" : "OFFLINE";
            int64_t ago = (esp_timer_get_time() - devices[i].last_seen_us) / 1000000LL;
            printf("    [%d] %s (%s) %s @ %s (%llds atras)\n",
                   i, devices[i].name, devices[i].id, online_str,
                   devices[i].ip, ago);
        }
    }
}

static void console_task(void *arg)
{
    (void)arg;

    printf("\n========================================\n");
    printf("  Bridge Console pronto!\n");
    printf("  Pressione 'h' para ajuda\n");
    printf("========================================\n");

    printf("\nbridge> ");
    fflush(stdout);

    while (1) {
        int c = getchar();
        if (c == EOF) continue;

        printf("%c\n", c);

        switch (c) {
        case 'l':
        case 'L':
            cmd_list();
            break;

        case 's':
        case 'S':
            cmd_status();
            break;

        case 'd':
        case 'D':
            cmd_detail();
            break;

        case 'b':
        case 'B':
            cmd_broadcast();
            break;

        case 'r':
        case 'R':
            printf("\n  Reiniciando...\n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
            break;

        case 'h':
        case 'H':
        case '?':
            print_help();
            break;

        case '\n':
        case '\r':
            break;

        default:
            printf("  Comando desconhecido: %c (0x%02x)\n", c, c);
            printf("  Pressione 'h' para ajuda\n");
            break;
        }

        printf("\nbridge> ");
        fflush(stdout);
    }
}

esp_err_t console_init(void)
{
    BaseType_t ret = xTaskCreate(
        console_task,
        "console",
        4096,
        NULL,
        tskIDLE_PRIORITY + 1,
        &s_console_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create console task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Console task started");
    return ESP_OK;
}
