#include "app_wifi_server.h"
#include "app_rmaker_gateway.h"
#include "app_device_registry.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include "mdns.h"
#include <app_network.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "wifi_server";
static httpd_handle_t s_server = NULL;

extern const char dashboard_html_start[] asm("_binary_dashboard_html_start");
extern const char dashboard_html_end[] asm("_binary_dashboard_html_end");
extern const char dashboard_css_start[] asm("_binary_dashboard_css_start");
extern const char dashboard_css_end[] asm("_binary_dashboard_css_end");

#define SCRATCH_BUFSIZE 8192

// UDP Discovery
#define DISCOVERY_PORT 5000
#define BROADCAST_INTERVAL_US (300 * 1000000)
#define BROADCAST_INITIAL_DELAY_US (30 * 1000000)

static TaskHandle_t s_udp_task = NULL;
static int s_udp_socket = -1;

// WebSocket monitor
static int s_ws_fd = -1;
static httpd_handle_t s_ws_hd = NULL;

static char s_bridge_ip[16] = "0.0.0.0";

static void ws_push_device_update(const char *id);

// UDP discovery IP cache
#define DISCOVERED_IP_TIMEOUT_US (300 * 1000000LL)

static discovered_ip_t s_discovered_ips[MAX_DISCOVERED_IPS];

static void cache_discovered_ip(const char *id, const char *ip)
{
    if (!id || !*id || !ip)
        return;
    for (int i = 0; i < MAX_DISCOVERED_IPS; i++)
    {
        if (strcmp(s_discovered_ips[i].id, id) == 0)
        {
            strncpy(s_discovered_ips[i].ip, ip, sizeof(s_discovered_ips[i].ip) - 1);
            s_discovered_ips[i].ip[sizeof(s_discovered_ips[i].ip) - 1] = '\0';
            s_discovered_ips[i].last_seen_us = esp_timer_get_time();
            return;
        }
    }
    for (int i = 0; i < MAX_DISCOVERED_IPS; i++)
    {
        if (s_discovered_ips[i].id[0] == '\0')
        {
            strncpy(s_discovered_ips[i].id, id, sizeof(s_discovered_ips[i].id) - 1);
            s_discovered_ips[i].id[sizeof(s_discovered_ips[i].id) - 1] = '\0';
            strncpy(s_discovered_ips[i].ip, ip, sizeof(s_discovered_ips[i].ip) - 1);
            s_discovered_ips[i].ip[sizeof(s_discovered_ips[i].ip) - 1] = '\0';
            s_discovered_ips[i].last_seen_us = esp_timer_get_time();
            return;
        }
    }
}

static const char *lookup_discovered_ip(const char *id)
{
    if (!id || !*id)
        return NULL;
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < MAX_DISCOVERED_IPS; i++)
    {
        if (s_discovered_ips[i].id[0] != '\0' &&
            strcmp(s_discovered_ips[i].id, id) == 0)
        {
            if (now - s_discovered_ips[i].last_seen_us < DISCOVERED_IP_TIMEOUT_US)
            {
                return s_discovered_ips[i].ip;
            }
            s_discovered_ips[i].id[0] = '\0';
            return NULL;
        }
    }
    return NULL;
}

static void cleanup_stale_discovered_ips(void)
{
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < MAX_DISCOVERED_IPS; i++)
    {
        if (s_discovered_ips[i].id[0] != '\0' &&
            now - s_discovered_ips[i].last_seen_us >= DISCOVERED_IP_TIMEOUT_US)
        {
            s_discovered_ips[i].id[0] = '\0';
        }
    }
}

void wifi_server_update_ip(const char *ip)
{
    strncpy(s_bridge_ip, ip, sizeof(s_bridge_ip) - 1);
    s_bridge_ip[sizeof(s_bridge_ip) - 1] = '\0';
    ESP_LOGI(TAG, "Cached gateway IP updated: %s", s_bridge_ip);
}

static void handle_udp_discovery(void)
{
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    char *buf = (char *)malloc(512);
    if (!buf)
        return;

    int len = recvfrom(s_udp_socket, buf, 511, MSG_DONTWAIT,
                       (struct sockaddr *)&src_addr, &addr_len);
    if (len > 0)
    {
        buf[len] = '\0';

        bool is_discovery = false;
        char discovered_id[MAX_DEVICE_ID_LEN] = {0};
        cJSON *root = cJSON_Parse(buf);
        if (root)
        {
            cJSON *svc = cJSON_GetObjectItem(root, "service");
            cJSON *disc = cJSON_GetObjectItem(root, "discover");
            if (svc && svc->valuestring &&
                (strcmp(svc->valuestring, "esp-bridge") == 0) &&
                disc && cJSON_IsTrue(disc))
            {
                is_discovery = true;
            }
            cJSON *id_item = cJSON_GetObjectItem(root, "id");
            if (id_item && id_item->valuestring)
            {
                strncpy(discovered_id, id_item->valuestring, sizeof(discovered_id) - 1);
            }
            cJSON_Delete(root);
        }

        if (discovered_id[0])
        {
            char src_ip_str[16];
            inet_ntop(AF_INET, &src_addr.sin_addr, src_ip_str, sizeof(src_ip_str));
            cache_discovered_ip(discovered_id, src_ip_str);
            ESP_LOGI(TAG, "Discovered device %s at IP %s", discovered_id, src_ip_str);
        }

        if (is_discovery && strcmp(s_bridge_ip, "0.0.0.0") != 0)
        {
            char resp[256];
            snprintf(resp, sizeof(resp),
                     "{\"service\":\"esp-bridge\",\"ip_sta\":\"%s\",\"http_port\":80}",
                     s_bridge_ip);
            sendto(s_udp_socket, resp, strlen(resp), 0,
                   (struct sockaddr *)&src_addr, addr_len);
            ESP_LOGI(TAG, "UDP discovery response sent to %s", inet_ntoa(src_addr.sin_addr));
        }
        else if (is_discovery)
        {
            ESP_LOGD(TAG, "UDP discovery request from %s (no IP yet, ignored)",
                     inet_ntoa(src_addr.sin_addr));
        }
    }

    free(buf);
}

static void send_udp_broadcast(void)
{
    if (strcmp(s_bridge_ip, "0.0.0.0") == 0)
        return;

    char resp[256];
    uint64_t uptime_s = esp_timer_get_time() / 1000000;
    snprintf(resp, sizeof(resp),
             "{\"service\":\"esp-bridge\",\"ip_sta\":\"%s\",\"http_port\":80,\"uptime_s\":%llu,\"re_register\":true}",
             s_bridge_ip, uptime_s);

    struct sockaddr_in bcast_addr;
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(DISCOVERY_PORT);
    bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(s_udp_socket, resp, strlen(resp), 0,
           (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
    ESP_LOGD(TAG, "UDP broadcast sent: IP %s", s_bridge_ip);
}

static void udp_discovery_task(void *pv)
{
    s_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_udp_socket < 0)
    {
        ESP_LOGE(TAG, "Failed to create UDP discovery socket");
        vTaskDelete(NULL);
        return;
    }

    int broadcast_enable = 1;
    setsockopt(s_udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DISCOVERY_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s_udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ESP_LOGE(TAG, "Failed to bind UDP socket to port %d", DISCOVERY_PORT);
        close(s_udp_socket);
        s_udp_socket = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP discovery listening on port %d", DISCOVERY_PORT);

    {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif)
        {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0)
            {
                snprintf(s_bridge_ip, sizeof(s_bridge_ip), IPSTR, IP2STR(&ip_info.ip));
                ESP_LOGI(TAG, "UDP discovery using cached IP: %s", s_bridge_ip);
            }
        }
    }

    int64_t last_broadcast = esp_timer_get_time() - BROADCAST_INTERVAL_US + BROADCAST_INITIAL_DELAY_US;
    while (1)
    {
        handle_udp_discovery();

        if (strcmp(s_bridge_ip, "0.0.0.0") == 0)
        {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif)
            {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0)
                {
                    snprintf(s_bridge_ip, sizeof(s_bridge_ip), IPSTR, IP2STR(&ip_info.ip));
                    ESP_LOGI(TAG, "UDP discovery IP acquired: %s", s_bridge_ip);
                }
            }
        }

        int64_t now = esp_timer_get_time();
        if (now - last_broadcast >= BROADCAST_INTERVAL_US)
        {
            last_broadcast = now;
            send_udp_broadcast();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void stop_udp_discovery(void)
{
    if (s_udp_task)
    {
        vTaskDelete(s_udp_task);
        s_udp_task = NULL;
    }
    if (s_udp_socket >= 0)
    {
        close(s_udp_socket);
        s_udp_socket = -1;
    }
}

static esp_err_t start_udp_discovery(void)
{
    if (s_udp_task)
        return ESP_OK;
    BaseType_t ret = xTaskCreate(udp_discovery_task, "udp_disc", 3072, NULL, 5, &s_udp_task);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create UDP discovery task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t register_device_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(SCRATCH_BUFSIZE);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_FAIL;
    }

    int received = 0, cur_len = 0;
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0)
        {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "read error");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    cJSON *name_item = cJSON_GetObjectItem(root, "name");

    if (!id_item || !id_item->valuestring || !type_item || !type_item->valuestring)
    {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id or type");
        return ESP_FAIL;
    }

    char id_buf[MAX_DEVICE_ID_LEN];
    char type_buf[32];
    char name_buf[MAX_DEVICE_NAME_LEN];

    strncpy(id_buf, id_item->valuestring, sizeof(id_buf) - 1);
    id_buf[sizeof(id_buf) - 1] = '\0';
    strncpy(type_buf, type_item->valuestring, sizeof(type_buf) - 1);
    type_buf[sizeof(type_buf) - 1] = '\0';

    if (name_item && name_item->valuestring) {
        strncpy(name_buf, name_item->valuestring, sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
    } else {
        name_buf[0] = '\0';
    }

    const char *id = id_buf;
    const char *type_str = type_buf;
    const char *name = name_buf[0] ? name_buf : id_buf;

    char client_ip[16] = "0.0.0.0";

    cJSON *ip_item = cJSON_GetObjectItem(root, "ip");
    if (ip_item && ip_item->valuestring && strlen(ip_item->valuestring) > 0)
    {
        strncpy(client_ip, ip_item->valuestring, sizeof(client_ip) - 1);
        client_ip[sizeof(client_ip) - 1] = '\0';
    }
    else
    {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int fd = httpd_req_to_sockfd(req);
        if (fd >= 0 && getpeername(fd, (struct sockaddr *)&addr, &addr_len) == 0)
        {
            inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
        }
        else
        {
            ESP_LOGW(TAG, "getpeername failed (fd=%d, errno=%d), trying UDP cache", fd, errno);
            const char *cached = lookup_discovered_ip(id);
            if (cached)
            {
                strncpy(client_ip, cached, sizeof(client_ip) - 1);
                client_ip[sizeof(client_ip) - 1] = '\0';
                ESP_LOGI(TAG, "Using UDP discovery cached IP for %s: %s", id, client_ip);
            }
        }
    }

    device_type_t type = device_type_from_string(type_str);
    if (type == DEVICE_TYPE_UNKNOWN)
    {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unsupported device type");
        return ESP_FAIL;
    }

    int slot = device_registry_register(id, type, name, client_ip);
    if (slot < 0)
    {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "registry full");
        return ESP_FAIL;
    }

    esp_err_t err = rmaker_gateway_device_add(id, type);
    if (err != ESP_OK)
    {
        device_registry_remove_device(id);
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "gateway add failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Device registered: %s (type: %s, slot: %d)", id, type_str, slot);

    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddNumberToObject(resp, "slot", slot);
    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    free(buf);
    return ESP_OK;
}

static esp_err_t remove_device_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(SCRATCH_BUFSIZE);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_FAIL;
    }

    int received = 0, cur_len = 0;
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0)
        {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "read error");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || !id_item->valuestring)
    {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id");
        return ESP_FAIL;
    }

    const char *id = id_item->valuestring;

    esp_err_t err = rmaker_gateway_device_remove(id);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "rmaker_gateway_device_remove failed: %s", esp_err_to_name(err));
    }

    err = device_registry_remove_device(id);
    if (err != ESP_OK)
    {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "device not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    free(buf);
    return ESP_OK;
}

static esp_err_t device_state_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(SCRATCH_BUFSIZE);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_FAIL;
    }

    int received = 0, cur_len = 0;
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0)
        {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "read error");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || !id_item->valuestring)
    {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id");
        return ESP_FAIL;
    }

    const char *id = id_item->valuestring;
    bridged_device_t *dev = device_registry_get_by_id(id);
    if (!dev)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "device not found");
        cJSON_Delete(root);
        free(buf);
        return ESP_FAIL;
    }

    cJSON *child = NULL;
    cJSON_ArrayForEach(child, root)
    {
        if (!child->string || strcmp(child->string, "id") == 0)
            continue;

        const char *key = child->string;
        char value[64];
        if (cJSON_IsBool(child))
        {
            snprintf(value, sizeof(value), "%s", child->valueint ? "true" : "false");
        }
        else if (cJSON_IsNumber(child))
        {
            snprintf(value, sizeof(value), "%g", child->valuedouble);
        }
        else if (cJSON_IsString(child))
        {
            snprintf(value, sizeof(value), "%s", child->valuestring);
        }
        else
        {
            continue;
        }

        device_registry_update_state(id, key, value);
        rmaker_gateway_device_update_state(id, key, value);
    }

    ws_push_device_update(id);

    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    free(buf);

    return ESP_OK;
}

static esp_err_t device_commands_handler(httpd_req_t *req)
{
    char id_str[MAX_DEVICE_ID_LEN] = {0};
    char query[256] = {0};
    bool found = false;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char *saveptr;
        char *param = strtok_r(query, "&", &saveptr);
        while (param)
        {
            if (strncmp(param, "id=", 3) == 0)
            {
                strncpy(id_str, param + 3, sizeof(id_str) - 1);
                found = true;
                break;
            }
            param = strtok_r(NULL, "&", &saveptr);
        }
    }

    if (!found)
    {
        int total_len = req->content_len;
        if (total_len > 0 && total_len < SCRATCH_BUFSIZE)
        {
            char *buf = (char *)malloc(SCRATCH_BUFSIZE);
            if (buf)
            {
                int received = 0, cur_len = 0;
                while (cur_len < total_len)
                {
                    received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
                    if (received <= 0)
                        break;
                    cur_len += received;
                }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
                if (root)
                {
                    cJSON *id_item = cJSON_GetObjectItem(root, "id");
                    if (id_item && id_item->valuestring)
                    {
                        strncpy(id_str, id_item->valuestring, sizeof(id_str) - 1);
                        found = true;
                    }
                    cJSON_Delete(root);
                }
                free(buf);
            }
        }
    }

    if (!found)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing device id");
        return ESP_FAIL;
    }

    bridged_device_t *dev = device_registry_get_by_id(id_str);
    if (!dev)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "device not found");
        return ESP_FAIL;
    }

    pending_command_t commands[MAX_PENDING_COMMANDS];
    int count = device_registry_get_commands(id_str, commands, MAX_PENDING_COMMANDS);

    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON *cmd_array = cJSON_AddArrayToObject(resp, "commands");
    for (int i = 0; i < count; i++)
    {
        cJSON *cmd = cJSON_CreateObject();
        cJSON_AddStringToObject(cmd, "cluster", commands[i].cluster);
        cJSON_AddStringToObject(cmd, "command", commands[i].command);
        cJSON_AddStringToObject(cmd, "data", commands[i].data);
        cJSON_AddItemToArray(cmd_array, cmd);
    }
    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);

    return ESP_OK;
}

static esp_err_t device_info_handler(httpd_req_t *req)
{
    char id_str[MAX_DEVICE_ID_LEN] = {0};
    char query[256] = {0};
    bool found = false;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char *saveptr;
        char *param = strtok_r(query, "&", &saveptr);
        while (param)
        {
            if (strncmp(param, "id=", 3) == 0)
            {
                strncpy(id_str, param + 3, sizeof(id_str) - 1);
                found = true;
                break;
            }
            param = strtok_r(NULL, "&", &saveptr);
        }
    }

    if (!found)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing device id");
        return ESP_FAIL;
    }

    bridged_device_t *dev = device_registry_get_by_id(id_str);
    if (!dev)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "device not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "id", dev->id);
    cJSON_AddStringToObject(resp, "name", dev->name);
    cJSON_AddStringToObject(resp, "ip", dev->ip);
    cJSON_AddStringToObject(resp, "type", device_type_to_string(dev->type));
    cJSON_AddBoolToObject(resp, "online", dev->online);
    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);

    return ESP_OK;
}

static esp_err_t device_heartbeat_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0 || total_len >= 256)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }

    char buf[256];
    int cur = 0;
    while (cur < total_len)
    {
        int r = httpd_req_recv(req, buf + cur, total_len - cur);
        if (r <= 0)
            break;
        cur += r;
    }
    buf[cur] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || !id_item->valuestring)
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id");
        return ESP_FAIL;
    }

    const char *id = id_item->valuestring;
    device_registry_mark_seen(id);
    ws_push_device_update(id);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t ping_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"rebooting...\"}");
    ESP_LOGW(TAG, "Reset requested via API, rebooting in 500ms...");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

static esp_err_t broadcast_handler(httpd_req_t *req)
{
    wifi_server_broadcast();
    ESP_LOGI(TAG, "Broadcast requested via API");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"broadcast sent\"}");
    return ESP_OK;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        s_ws_hd = req->handle;
        s_ws_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "WS client connected fd=%d", s_ws_fd);
        return ESP_OK;
    }

    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK)
    {
        s_ws_fd = -1;
        s_ws_hd = NULL;
        ESP_LOGI(TAG, "WS client disconnected");
    }
    return ret;
}

static void ws_push_device_update(const char *id)
{
    if (s_ws_fd < 0 || !s_ws_hd)
        return;
    bridged_device_t *dev = device_registry_get_by_id(id);
    if (!dev || !dev->registered)
        return;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "t", "device_update");
    cJSON_AddStringToObject(resp, "id", dev->id);
    cJSON_AddStringToObject(resp, "name", dev->name);
    cJSON_AddStringToObject(resp, "type", device_type_to_string(dev->type));
    cJSON_AddBoolToObject(resp, "online", dev->online);
    char state_buf[128];
    if (device_registry_get_state_json(dev->id, state_buf, sizeof(state_buf)) == ESP_OK
        && strcmp(state_buf, "{}") != 0)
    {
        cJSON *state = cJSON_Parse(state_buf);
        if (state)
            cJSON_AddItemToObject(resp, "state", state);
    }
    const char *payload = cJSON_Print(resp);
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)payload,
        .len = (size_t)strlen(payload)};
    httpd_ws_send_frame_async(s_ws_hd, s_ws_fd, &frame);
    free((void *)payload);
    cJSON_Delete(resp);
}

static void ws_monitor_task(void *pv)
{
    while (1)
    {
        if (s_ws_fd >= 0 && s_ws_hd)
        {
            if (httpd_ws_get_fd_info(s_ws_hd, s_ws_fd) == HTTPD_WS_CLIENT_INVALID)
            {
                s_ws_fd = -1;
                s_ws_hd = NULL;
                ESP_LOGI(TAG, "WS client disconnected");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            uint64_t uptime_s = esp_timer_get_time() / 1000000;
            char keepalive[128];
            int len = snprintf(keepalive, sizeof(keepalive),
                               "{\"t\":\"keepalive\",\"uptime_s\":%llu,\"ip\":\"%s\"}",
                               uptime_s, s_bridge_ip);
            httpd_ws_frame_t frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)keepalive,
                .len = (size_t)len};
            esp_err_t ret = httpd_ws_send_frame_async(s_ws_hd, s_ws_fd, &frame);
            if (ret != ESP_OK)
            {
                s_ws_fd = -1;
                s_ws_hd = NULL;
                ESP_LOGI(TAG, "WS send failed, client removed");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(NULL);
}

static esp_err_t send_embedded_asset(httpd_req_t *req, const char *start, const char *end, const char *content_type)
{
    httpd_resp_set_type(req, content_type);
    size_t len = (size_t)(end - start);
    httpd_resp_send(req, start, len);
    return ESP_OK;
}

static esp_err_t dashboard_html_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, dashboard_html_start, dashboard_html_end, "text/html");
}

static esp_err_t dashboard_css_handler(httpd_req_t *req)
{
    return send_embedded_asset(req, dashboard_css_start, dashboard_css_end, "text/css");
}

static void add_ip_to_json(cJSON *obj, const char *key, esp_ip4_addr_t addr)
{
    char buf[16];
    snprintf(buf, sizeof(buf), IPSTR, IP2STR(&addr));
    cJSON_AddStringToObject(obj, key, buf);
}

static esp_err_t gateway_info_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "ip", s_bridge_ip);
    cJSON_AddStringToObject(resp, "version", FW_VERSION);
    cJSON_AddNumberToObject(resp, "uptime_s", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(resp, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(resp, "min_free_heap", esp_get_minimum_free_heap_size());

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            add_ip_to_json(resp, "gateway", ip_info.gw);
            add_ip_to_json(resp, "netmask", ip_info.netmask);
        }
        esp_netif_dns_info_t dns;
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK && dns.ip.u_addr.ip4.addr != 0) {
            add_ip_to_json(resp, "dns1", dns.ip.u_addr.ip4);
        }
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns) == ESP_OK && dns.ip.u_addr.ip4.addr != 0) {
            add_ip_to_json(resp, "dns2", dns.ip.u_addr.ip4);
        }
    }

    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);
    return ESP_OK;
}

static esp_err_t devices_list_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *resp = cJSON_CreateObject();
    cJSON *dev_array = cJSON_AddArrayToObject(resp, "devices");

    int count = 0;
    bridged_device_t *devices = device_registry_get_all(&count);

    for (int i = 0; i < count; i++)
    {
        if (devices[i].registered)
        {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "id", devices[i].id);
            cJSON_AddStringToObject(item, "name", devices[i].name);
            cJSON_AddStringToObject(item, "ip", devices[i].ip);
            cJSON_AddStringToObject(item, "type", device_type_to_string(devices[i].type));
            cJSON_AddBoolToObject(item, "online", devices[i].online);

            int64_t elapsed_s = (esp_timer_get_time() - devices[i].last_seen_us) / 1000000;
            cJSON_AddNumberToObject(item, "last_seen_elapsed_s", elapsed_s);

            char state_buf[128];
            if (device_registry_get_state_json(devices[i].id, state_buf, sizeof(state_buf)) == ESP_OK
                && strcmp(state_buf, "{}") != 0)
            {
                cJSON *state = cJSON_Parse(state_buf);
                if (state)
                {
                    cJSON_AddItemToObject(item, "state", state);
                }
            }

            cJSON_AddItemToArray(dev_array, item);
        }
    }

    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);

    return ESP_OK;
}

static esp_err_t qrcode_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    cJSON *resp = cJSON_CreateObject();

    char *service_name = app_network_get_device_service_name();
    char *pop = app_network_get_device_pop_default();

    cJSON_AddStringToObject(resp, "service_name", service_name ? service_name : "");
    cJSON_AddStringToObject(resp, "pop", pop ? pop : "");
#ifdef CONFIG_APP_NETWORK_PROV_TRANSPORT_BLE
    const char *transport = "ble";
#else
    const char *transport = "softap";
#endif
    cJSON_AddStringToObject(resp, "transport", transport);
    cJSON_AddStringToObject(resp, "ver", "v1");

    char qr_payload[256];
    if (pop)
    {
        snprintf(qr_payload, sizeof(qr_payload),
                 "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 service_name ? service_name : "", pop, transport);
    }
    else
    {
        snprintf(qr_payload, sizeof(qr_payload),
                 "{\"ver\":\"v1\",\"name\":\"%s\",\"transport\":\"%s\"}",
                 service_name ? service_name : "", transport);
    }
    cJSON_AddStringToObject(resp, "qr", qr_payload);

    free(service_name);
    free(pop);

    const char *resp_str = cJSON_Print(resp);
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);

    return ESP_OK;
}

static esp_err_t ota_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (!partition)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    int received;

    while (remaining > 0)
    {
        int to_read = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        received = httpd_req_recv(req, buf, to_read);
        if (received <= 0)
        {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read error");
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK)
        {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot failed");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Firmware updated, rebooting...\"}");
    ESP_LOGI(TAG, "OTA successful, rebooting in 1s");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

void wifi_server_broadcast(void)
{
    send_udp_broadcast();
    ESP_LOGI(TAG, "Manual UDP broadcast sent");
}

int wifi_server_get_discovered_ips(discovered_ip_t *ips, int max)
{
    int count = 0;
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < MAX_DISCOVERED_IPS && count < max; i++)
    {
        if (s_discovered_ips[i].id[0] != '\0' &&
            now - s_discovered_ips[i].last_seen_us < DISCOVERED_IP_TIMEOUT_US)
        {
            memcpy(&ips[count], &s_discovered_ips[i], sizeof(discovered_ip_t));
            count++;
        }
    }
    return count;
}

esp_err_t wifi_server_start(void)
{
    if (s_server)
    {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.max_open_sockets = 20;
    config.keep_alive_enable = false;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t register_uri = {
        .uri = "/api/device/register",
        .method = HTTP_POST,
        .handler = register_device_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &register_uri);

    httpd_uri_t remove_uri = {
        .uri = "/api/device/remove",
        .method = HTTP_POST,
        .handler = remove_device_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &remove_uri);

    httpd_uri_t state_uri = {
        .uri = "/api/device/state",
        .method = HTTP_POST,
        .handler = device_state_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &state_uri);

    httpd_uri_t commands_uri = {
        .uri = "/api/device/commands",
        .method = HTTP_GET,
        .handler = device_commands_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &commands_uri);

    httpd_uri_t commands_post_uri = {
        .uri = "/api/device/commands",
        .method = HTTP_POST,
        .handler = device_commands_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &commands_post_uri);

    httpd_uri_t info_uri = {
        .uri = "/api/device/info",
        .method = HTTP_GET,
        .handler = device_info_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &info_uri);

    httpd_uri_t list_uri = {
        .uri = "/api/devices",
        .method = HTTP_GET,
        .handler = devices_list_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &list_uri);

    httpd_uri_t gateway_info_uri = {
        .uri = "/api/gateway/info",
        .method = HTTP_GET,
        .handler = gateway_info_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &gateway_info_uri);

    httpd_uri_t ping_uri = {
        .uri = "/api/ping",
        .method = HTTP_GET,
        .handler = ping_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &ping_uri);

    httpd_uri_t heartbeat_uri = {
        .uri = "/api/device/heartbeat",
        .method = HTTP_POST,
        .handler = device_heartbeat_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &heartbeat_uri);

    httpd_uri_t reset_uri = {
        .uri = "/api/gateway/reset",
        .method = HTTP_POST,
        .handler = reset_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &reset_uri);

    httpd_uri_t broadcast_uri = {
        .uri = "/api/gateway/broadcast",
        .method = HTTP_POST,
        .handler = broadcast_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &broadcast_uri);

    httpd_uri_t qrcode_uri = {
        .uri = "/api/qrcode",
        .method = HTTP_GET,
        .handler = qrcode_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &qrcode_uri);

    httpd_uri_t ota_uri = {
        .uri = "/api/ota",
        .method = HTTP_POST,
        .handler = ota_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &ota_uri);

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = dashboard_html_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &root_uri);

    httpd_uri_t css_uri = {
        .uri = "/dashboard.css",
        .method = HTTP_GET,
        .handler = dashboard_css_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &css_uri);

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true};
    httpd_register_uri_handler(s_server, &ws_uri);

    xTaskCreatePinnedToCore(ws_monitor_task, "ws_monitor", 3072, NULL, 5, NULL, tskNO_AFFINITY);

    err = start_udp_discovery();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to start UDP discovery (non-fatal)");
    }

    esp_err_t mdns_err = mdns_init();
    if (mdns_err == ESP_OK)
    {
        mdns_hostname_set("espbridge");
        mdns_service_add("espbridge-http", "_http", "_tcp", 80, NULL, 0);
        ESP_LOGI(TAG, "mDNS: espbridge.local");
    }
    else
    {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(mdns_err));
    }

    ESP_LOGI(TAG, "HTTP REST server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t wifi_server_stop(void)
{
    stop_udp_discovery();
    s_ws_fd = -1;
    s_ws_hd = NULL;
    if (s_server)
    {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}
