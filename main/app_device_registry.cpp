#include "app_device_registry.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>

static const char *TAG = "device_registry";

static bridged_device_t s_devices[MAX_BRIDGED_DEVICES];
static int s_device_count = 0;
static int s_loaded_count = 0;

static SemaphoreHandle_t s_registry_mutex = NULL;

#define NVS_KEY_DEVICES "devices"

typedef struct __attribute__((packed)) {
    char id[MAX_DEVICE_ID_LEN];
    char name[MAX_DEVICE_NAME_LEN];
    char ip[16];
    int32_t type;
    char state_json[MAX_DEVICE_STATE_LEN];
} persisted_device_t;

static void device_registry_save(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_DEVICE_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open for save failed: %s", esp_err_to_name(err));
        return;
    }

    uint8_t count = 0;
    for (int i = 0; i < MAX_BRIDGED_DEVICES; i++) {
        if (s_devices[i].registered) count++;
    }

    size_t blob_size = sizeof(uint8_t) + count * sizeof(persisted_device_t);
    uint8_t *blob = (uint8_t *)malloc(blob_size);
    if (!blob) {
        nvs_close(nvs);
        return;
    }
    blob[0] = count;
    persisted_device_t *pdevs = (persisted_device_t *)(blob + sizeof(uint8_t));
    int idx = 0;
    for (int i = 0; i < MAX_BRIDGED_DEVICES; i++) {
        if (s_devices[i].registered) {
            strncpy(pdevs[idx].id, s_devices[i].id, MAX_DEVICE_ID_LEN - 1);
            strncpy(pdevs[idx].name, s_devices[i].name, MAX_DEVICE_NAME_LEN - 1);
            strncpy(pdevs[idx].ip, s_devices[i].ip, sizeof(pdevs[idx].ip) - 1);
            pdevs[idx].type = (int32_t)s_devices[i].type;
            strncpy(pdevs[idx].state_json, s_devices[i].state_json, MAX_DEVICE_STATE_LEN - 1);
            idx++;
        }
    }

    err = nvs_set_blob(nvs, NVS_KEY_DEVICES, blob, blob_size);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    free(blob);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS save failed: %s", esp_err_to_name(err));
    }
    nvs_close(nvs);
}

static void device_registry_load(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_DEVICE_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS open for load failed (first boot?): %s", esp_err_to_name(err));
        return;
    }

    uint8_t count = 0;
    size_t required_size = sizeof(count);
    err = nvs_get_blob(nvs, NVS_KEY_DEVICES, &count, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs);
        return;
    }

    size_t blob_size = sizeof(uint8_t) + count * sizeof(persisted_device_t);
    uint8_t *blob = (uint8_t *)malloc(blob_size);
    if (!blob) {
        nvs_close(nvs);
        return;
    }

    size_t actual_size = blob_size;
    err = nvs_get_blob(nvs, NVS_KEY_DEVICES, blob, &actual_size);
    if (err != ESP_OK || actual_size < sizeof(uint8_t)) {
        free(blob);
        nvs_close(nvs);
        return;
    }

    uint8_t actual_count = blob[0];
    if (actual_count > MAX_BRIDGED_DEVICES) actual_count = MAX_BRIDGED_DEVICES;

    for (int i = 0; i < actual_count; i++) {
        persisted_device_t *pdev = (persisted_device_t *)(blob + sizeof(uint8_t) + i * sizeof(persisted_device_t));
        bridged_device_t *dev = &s_devices[i];
        strncpy(dev->id, pdev->id, MAX_DEVICE_ID_LEN - 1);
        strncpy(dev->name, pdev->name, MAX_DEVICE_NAME_LEN - 1);
        strncpy(dev->ip, pdev->ip, sizeof(dev->ip) - 1);
        dev->type = (device_type_t)pdev->type;
        dev->registered = true;
        dev->rmaker_device_hdl = NULL;
        dev->pending_command_count = 0;
        dev->online = false;
        dev->last_seen_us = 0;
        strncpy(dev->state_json, pdev->state_json, MAX_DEVICE_STATE_LEN - 1);
    }

    s_device_count = actual_count;
    s_loaded_count = actual_count;
    free(blob);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Loaded %d devices from NVS", s_loaded_count);
}

static int find_device_slot(void)
{
    for (int i = 0; i < MAX_BRIDGED_DEVICES; i++) {
        if (!s_devices[i].registered) {
            return i;
        }
    }
    return -1;
}

static int find_device_by_id(const char *id)
{
    for (int i = 0; i < MAX_BRIDGED_DEVICES; i++) {
        if (s_devices[i].registered && strcmp(s_devices[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

device_type_t device_type_from_string(const char *type_str)
{
    if (strcmp(type_str, "onoff") == 0) return DEVICE_TYPE_ON_OFF;
    if (strcmp(type_str, "dimmable") == 0) return DEVICE_TYPE_DIMMABLE;
    if (strcmp(type_str, "temperature") == 0) return DEVICE_TYPE_TEMPERATURE_SENSOR;
    if (strcmp(type_str, "humidity") == 0) return DEVICE_TYPE_HUMIDITY_SENSOR;
    if (strcmp(type_str, "contact") == 0) return DEVICE_TYPE_CONTACT_SENSOR;
    if (strcmp(type_str, "occupancy") == 0) return DEVICE_TYPE_OCCUPANCY_SENSOR;
    if (strcmp(type_str, "light_sensor") == 0) return DEVICE_TYPE_LIGHT_SENSOR;
    if (strcmp(type_str, "tanque") == 0) return DEVICE_TYPE_TANQUE;
    if (strcmp(type_str, "gas") == 0) return DEVICE_TYPE_GAS_SENSOR;
    if (strcmp(type_str, "rain") == 0) return DEVICE_TYPE_RAIN_SENSOR;
    if (strcmp(type_str, "electricity") == 0) return DEVICE_TYPE_ELECTRICITY;
    return DEVICE_TYPE_UNKNOWN;
}

const char *device_type_to_string(device_type_t type)
{
    switch (type) {
    case DEVICE_TYPE_ON_OFF: return "onoff";
    case DEVICE_TYPE_DIMMABLE: return "dimmable";
    case DEVICE_TYPE_TEMPERATURE_SENSOR: return "temperature";
    case DEVICE_TYPE_HUMIDITY_SENSOR: return "humidity";
    case DEVICE_TYPE_CONTACT_SENSOR: return "contact";
    case DEVICE_TYPE_OCCUPANCY_SENSOR: return "occupancy";
    case DEVICE_TYPE_LIGHT_SENSOR: return "light_sensor";
    case DEVICE_TYPE_TANQUE: return "tanque";
    case DEVICE_TYPE_GAS_SENSOR: return "gas";
    case DEVICE_TYPE_RAIN_SENSOR: return "rain";
    case DEVICE_TYPE_ELECTRICITY: return "electricity";
    default: return "unknown";
    }
}

esp_err_t device_registry_init(void)
{
    memset(s_devices, 0, sizeof(s_devices));
    s_device_count = 0;
    s_loaded_count = 0;
    s_registry_mutex = xSemaphoreCreateMutex();
    if (!s_registry_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    device_registry_load();
    ESP_LOGI(TAG, "Device registry initialized, max %d devices (%d restored from NVS)",
             MAX_BRIDGED_DEVICES, s_loaded_count);
    return ESP_OK;
}

int device_registry_register(const char *id, device_type_t type, const char *name, const char *ip)
{
    if (!s_registry_mutex) return -1;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);

    int existing = find_device_by_id(id);
    if (existing >= 0) {
        bridged_device_t *dev = &s_devices[existing];
        strncpy(dev->name, name ? name : id, MAX_DEVICE_NAME_LEN - 1);
        dev->name[MAX_DEVICE_NAME_LEN - 1] = '\0';
        strncpy(dev->ip, ip ? ip : "0.0.0.0", sizeof(dev->ip) - 1);
        dev->ip[sizeof(dev->ip) - 1] = '\0';
        dev->type = type;
        dev->registered = true;
        dev->online = true;
        dev->last_seen_us = esp_timer_get_time();
        ESP_LOGW(TAG, "Device %s already registered, refreshed metadata", id);
        xSemaphoreGive(s_registry_mutex);
        device_registry_save();
        return existing;
    }

    int slot = find_device_slot();
    if (slot < 0) {
        ESP_LOGE(TAG, "No slots available for device %s", id);
        xSemaphoreGive(s_registry_mutex);
        return -1;
    }

    bridged_device_t *dev = &s_devices[slot];
    strncpy(dev->id, id, MAX_DEVICE_ID_LEN - 1);
    dev->id[MAX_DEVICE_ID_LEN - 1] = '\0';
    strncpy(dev->name, name ? name : id, MAX_DEVICE_NAME_LEN - 1);
    dev->name[MAX_DEVICE_NAME_LEN - 1] = '\0';
    strncpy(dev->ip, ip ? ip : "0.0.0.0", sizeof(dev->ip) - 1);
    dev->ip[sizeof(dev->ip) - 1] = '\0';
    dev->type = type;
    dev->registered = true;
    dev->rmaker_device_hdl = NULL;
    dev->pending_command_count = 0;
    dev->online = true;
    dev->last_seen_us = esp_timer_get_time();
    dev->state_json[0] = '\0';
    s_device_count++;

    ESP_LOGI(TAG, "Registered device: %s (type: %s, slot: %d)", id, device_type_to_string(type), slot);
    xSemaphoreGive(s_registry_mutex);
    device_registry_save();
    return slot;
}

bridged_device_t *device_registry_get_by_id(const char *id)
{
    if (!s_registry_mutex) return NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx < 0) {
        xSemaphoreGive(s_registry_mutex);
        return NULL;
    }
    xSemaphoreGive(s_registry_mutex);
    return &s_devices[idx];
}

bridged_device_t *device_registry_get_all(int *count)
{
    if (count) *count = s_device_count;
    return s_devices;
}

esp_err_t device_registry_update_state(const char *id, const char *key, const char *value)
{
    if (!s_registry_mutex) return ESP_FAIL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx < 0) {
        xSemaphoreGive(s_registry_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    bridged_device_t *dev = &s_devices[idx];
    char current[MAX_DEVICE_STATE_LEN];
    char next_state[MAX_DEVICE_STATE_LEN];
    strncpy(current, dev->state_json, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';
    next_state[0] = '\0';

    bool replaced = false;
    char *token = strtok(current, "|");
    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            const char *entry_key = token;
            const char *entry_value = (strcmp(entry_key, key) == 0) ? value : (eq + 1);

            char entry[MAX_DEVICE_STATE_LEN];
            int written = snprintf(entry, sizeof(entry), "%s=%s", entry_key, entry_value);
            if (written > 0 && written < (int)sizeof(entry)) {
                size_t cur_len = strlen(next_state);
                size_t add_len = strlen(entry);
                if (cur_len > 0) {
                    if (cur_len + 1 >= sizeof(next_state)) {
                        xSemaphoreGive(s_registry_mutex);
                        return ESP_ERR_NO_MEM;
                    }
                    next_state[cur_len++] = '|';
                    next_state[cur_len] = '\0';
                }
                if (cur_len + add_len >= sizeof(next_state)) {
                    xSemaphoreGive(s_registry_mutex);
                    return ESP_ERR_NO_MEM;
                }
                memcpy(next_state + cur_len, entry, add_len + 1);
            }

            if (strcmp(entry_key, key) == 0) {
                replaced = true;
            }
        }
        token = strtok(NULL, "|");
    }

    if (!replaced) {
        char entry[MAX_DEVICE_STATE_LEN];
        int written = snprintf(entry, sizeof(entry), "%s=%s", key, value);
        if (written <= 0 || written >= (int)sizeof(entry)) {
            xSemaphoreGive(s_registry_mutex);
            return ESP_ERR_NO_MEM;
        }

        size_t cur_len = strlen(next_state);
        size_t add_len = strlen(entry);
        if (cur_len > 0) {
            if (cur_len + 1 >= sizeof(next_state)) {
                xSemaphoreGive(s_registry_mutex);
                return ESP_ERR_NO_MEM;
            }
            next_state[cur_len++] = '|';
            next_state[cur_len] = '\0';
        }
        if (cur_len + add_len >= sizeof(next_state)) {
            xSemaphoreGive(s_registry_mutex);
            return ESP_ERR_NO_MEM;
        }
        memcpy(next_state + cur_len, entry, add_len + 1);
    }

    strncpy(dev->state_json, next_state, sizeof(dev->state_json) - 1);
    dev->state_json[sizeof(dev->state_json) - 1] = '\0';

    dev->online = true;
    dev->last_seen_us = esp_timer_get_time();

    ESP_LOGI(TAG, "Device %s state update: %s = %s", id, key, value);
    xSemaphoreGive(s_registry_mutex);
    return ESP_OK;
}

esp_err_t device_registry_add_command(const char *id, const char *cluster, const char *command, const char *data)
{
    if (!s_registry_mutex) return ESP_FAIL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx < 0) {
        xSemaphoreGive(s_registry_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    bridged_device_t *dev = &s_devices[idx];
    if (dev->pending_command_count >= MAX_PENDING_COMMANDS) {
        ESP_LOGW(TAG, "Command queue full for device %s", dev->id);
        xSemaphoreGive(s_registry_mutex);
        return ESP_ERR_NO_MEM;
    }

    pending_command_t *pc = &dev->pending_commands[dev->pending_command_count++];
    strncpy(pc->cluster, cluster, sizeof(pc->cluster) - 1);
    pc->cluster[sizeof(pc->cluster) - 1] = '\0';
    strncpy(pc->command, command, sizeof(pc->command) - 1);
    pc->command[sizeof(pc->command) - 1] = '\0';
    if (data) strncpy(pc->data, data, MAX_COMMAND_DATA_LEN - 1);
    else pc->data[0] = '\0';
    pc->data[MAX_COMMAND_DATA_LEN - 1] = '\0';

    ESP_LOGI(TAG, "Command queued for %s: %s/%s", dev->id, cluster, command);
    xSemaphoreGive(s_registry_mutex);
    return ESP_OK;
}

int device_registry_get_commands(const char *id, pending_command_t *commands, int max_commands)
{
    if (!s_registry_mutex) return 0;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx < 0) {
        xSemaphoreGive(s_registry_mutex);
        return 0;
    }

    bridged_device_t *dev = &s_devices[idx];
    int count = (dev->pending_command_count < max_commands) ? dev->pending_command_count : max_commands;
    for (int i = 0; i < count; i++) {
        memcpy(&commands[i], &dev->pending_commands[i], sizeof(pending_command_t));
    }
    dev->pending_command_count = 0;

    xSemaphoreGive(s_registry_mutex);
    return count;
}

esp_err_t device_registry_remove_device(const char *id)
{
    if (!s_registry_mutex) return ESP_FAIL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx < 0) {
        xSemaphoreGive(s_registry_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memset(&s_devices[idx], 0, sizeof(bridged_device_t));
    s_device_count--;
    ESP_LOGI(TAG, "Removed device: %s", id);
    xSemaphoreGive(s_registry_mutex);
    device_registry_save();
    return ESP_OK;
}

const char *device_registry_get_state_json(const char *id)
{
    if (!s_registry_mutex) return "{}";
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx < 0) {
        xSemaphoreGive(s_registry_mutex);
        return "{}";
    }

    static char json_buf[MAX_DEVICE_STATE_LEN + 16];
    const char *state = s_devices[idx].state_json;
    if (strlen(state) == 0) {
        xSemaphoreGive(s_registry_mutex);
        return "{}";
    }

    char tmp[MAX_DEVICE_STATE_LEN];
    strncpy(tmp, state, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    xSemaphoreGive(s_registry_mutex);

    json_buf[0] = '{';
    int pos = 1;
    char *token = strtok(tmp, "|");
    while (token && pos < (int)sizeof(json_buf) - 4) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            const char *k = token;
            const char *v = eq + 1;
            bool is_bool = (strcmp(v, "true") == 0 || strcmp(v, "false") == 0);
            bool is_num = true;
            for (const char *p = v; *p; p++) {
                if (*p != '.' && *p != '-' && (*p < '0' || *p > '9')) { is_num = false; break; }
            }
            if (pos > 1) json_buf[pos++] = ',';
            if (is_bool || is_num) {
                pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, "\"%s\":%s", k, v);
            } else {
                pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, "\"%s\":\"%s\"", k, v);
            }
        }
        token = strtok(NULL, "|");
    }

    if (pos < (int)sizeof(json_buf)) {
        json_buf[pos] = '}';
        json_buf[pos + 1] = '\0';
    } else {
        json_buf[sizeof(json_buf) - 2] = '}';
        json_buf[sizeof(json_buf) - 1] = '\0';
    }

    return json_buf;
}

void device_registry_mark_seen(const char *id)
{
    if (!s_registry_mutex) return;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx >= 0) {
        s_devices[idx].online = true;
        s_devices[idx].last_seen_us = esp_timer_get_time();
    }
    xSemaphoreGive(s_registry_mutex);
}

void device_registry_set_rmaker_handle(const char *id, void *rmaker_dev)
{
    if (!s_registry_mutex) return;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    if (idx >= 0) {
        s_devices[idx].rmaker_device_hdl = rmaker_dev;
    }
    xSemaphoreGive(s_registry_mutex);
}

void *device_registry_get_rmaker_handle(const char *id)
{
    if (!s_registry_mutex) return NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    int idx = find_device_by_id(id);
    void *hdl = NULL;
    if (idx >= 0) {
        hdl = s_devices[idx].rmaker_device_hdl;
    }
    xSemaphoreGive(s_registry_mutex);
    return hdl;
}

int device_registry_get_loaded_count(void)
{
    return s_loaded_count;
}
