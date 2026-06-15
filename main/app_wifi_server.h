#pragma once

#include <esp_err.h>
#include <stdint.h>

#define MAX_DISCOVERED_IPS 8
#define MAX_DISCOVERED_ID_LEN 48

typedef struct {
    char id[MAX_DISCOVERED_ID_LEN];
    char ip[16];
    int64_t last_seen_us;
} discovered_ip_t;

esp_err_t wifi_server_start(void);
esp_err_t wifi_server_stop(void);
void wifi_server_update_ip(const char *ip);
void wifi_server_broadcast(void);
int wifi_server_get_discovered_ips(discovered_ip_t *ips, int max);
