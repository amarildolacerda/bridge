#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

// C++ types used by mock implementations
static int64_t s_mock_time_us = 1000000;
static int s_mutex_counter = 0;

struct NvsEntry {
    std::vector<uint8_t> data;
};
static std::map<std::string, NvsEntry> s_nvs_store;
static uint32_t s_next_handle = 100;
static std::map<uint32_t, std::string> s_open_namespaces;

extern "C" {

// --- esp_err ---
const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
    case ESP_OK: return "ESP_OK";
    case ESP_FAIL: return "ESP_FAIL";
    case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
    default: return "ESP_ERR_UNKNOWN";
    }
}

// --- esp_timer ---
int64_t esp_timer_get_time(void)
{
    return s_mock_time_us;
}

void mock_esp_timer_set_time(int64_t t)
{
    s_mock_time_us = t;
}

void mock_esp_timer_advance(int64_t us)
{
    s_mock_time_us += us;
}

unsigned long long mock_esp_timer_get_time(void)
{
    return (unsigned long long)s_mock_time_us;
}

// --- FreeRTOS semaphore mock ---
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (SemaphoreHandle_t)(uintptr_t)(++s_mutex_counter);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t)
{
    return pdPASS;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t)
{
    return pdPASS;
}

void vSemaphoreDelete(SemaphoreHandle_t) {}

// --- NVS mock ---
esp_err_t nvs_open(const char *name, nvs_open_mode_t, nvs_handle_t *out_handle)
{
    uint32_t h = s_next_handle++;
    s_open_namespaces[h] = name;
    *out_handle = h;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    s_open_namespaces.erase((uint32_t)handle);
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    auto ns_it = s_open_namespaces.find((uint32_t)handle);
    if (ns_it == s_open_namespaces.end()) return ESP_ERR_NOT_FOUND;
    std::string full_key = ns_it->second + ":" + key;
    NvsEntry entry;
    entry.data.resize(length);
    memcpy(entry.data.data(), value, length);
    s_nvs_store[full_key] = std::move(entry);
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    auto ns_it = s_open_namespaces.find((uint32_t)handle);
    if (ns_it == s_open_namespaces.end()) return ESP_ERR_NOT_FOUND;
    std::string full_key = ns_it->second + ":" + key;
    auto it = s_nvs_store.find(full_key);
    if (it == s_nvs_store.end()) return ESP_ERR_NOT_FOUND;
    if (out_value == NULL) {
        *length = it->second.data.size();
        return ESP_OK;
    }
    size_t copy_len = *length < it->second.data.size() ? *length : it->second.data.size();
    memcpy(out_value, it->second.data.data(), copy_len);
    *length = it->second.data.size();
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t)
{
    return ESP_OK;
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    auto ns_it = s_open_namespaces.find((uint32_t)handle);
    if (ns_it == s_open_namespaces.end()) return ESP_ERR_NOT_FOUND;
    std::string prefix = ns_it->second + ":";
    auto it = s_nvs_store.begin();
    while (it != s_nvs_store.end()) {
        if (it->first.substr(0, prefix.size()) == prefix) {
            it = s_nvs_store.erase(it);
        } else {
            ++it;
        }
    }
    return ESP_OK;
}

void mock_nvs_clear(void)
{
    s_nvs_store.clear();
    s_open_namespaces.clear();
}

} // extern "C"
