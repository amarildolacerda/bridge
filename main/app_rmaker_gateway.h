#pragma once

#include <esp_err.h>
#include "app_device_registry.h"

esp_err_t rmaker_gateway_init(void);
esp_err_t rmaker_gateway_device_add(const char *id, device_type_t type);
esp_err_t rmaker_gateway_device_remove(const char *id);
esp_err_t rmaker_gateway_device_update_state(const char *id, const char *key, const char *value);
