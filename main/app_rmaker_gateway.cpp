#include "app_rmaker_gateway.h"
#include <esp_log.h>
#include <esp_err.h>
#include <stdlib.h>
#include <string.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>

static const char *TAG = "rmaker_gateway";

static esp_rmaker_node_t *s_node = NULL;
static bool s_initialized = false;

static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    const char *dev_id = (const char *)priv_data;
    if (!dev_id) {
        ESP_LOGE(TAG, "write_cb: no priv_data");
        return ESP_FAIL;
    }

    const char *param_name = esp_rmaker_param_get_name(param);
    char value_str[64];
    const char *cluster = "unknown";
    const char *command = "unknown";

    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        cluster = "onoff";
        command = "set_onoff";
        snprintf(value_str, sizeof(value_str), "%d", val.val.b ? 1 : 0);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
        cluster = "levelcontrol";
        command = "set_level";
        snprintf(value_str, sizeof(value_str), "%d", val.val.i);
    } else {
        ESP_LOGW(TAG, "Unhandled param %s for device %s", param_name, dev_id);
        return ESP_OK;
    }

    esp_err_t err = device_registry_add_command(dev_id, cluster, command, value_str);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Command %s/%s=%s queued for %s", cluster, command, value_str, dev_id);
    } else {
        ESP_LOGW(TAG, "Failed to queue command for %s: %s", dev_id, esp_err_to_name(err));
    }

    return ESP_OK;
}

esp_err_t rmaker_gateway_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    s_node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Gateway", "esp.device.bridge");
    if (!s_node) {
        ESP_LOGE(TAG, "Failed to init RainMaker node");
        return ESP_FAIL;
    }

    esp_rmaker_ota_enable_default();
    esp_rmaker_timezone_service_enable();

    s_initialized = true;
    ESP_LOGI(TAG, "RainMaker gateway initialized");
    return ESP_OK;
}

esp_err_t rmaker_gateway_device_add(const char *id, device_type_t type)
{
    if (!s_initialized || !s_node || !id) return ESP_FAIL;

    bridged_device_t *dev = device_registry_get_by_id(id);
    if (!dev) {
        ESP_LOGE(TAG, "Device %s not found in registry", id);
        return ESP_ERR_NOT_FOUND;
    }

    if (dev->rmaker_device_hdl) {
        ESP_LOGW(TAG, "Device %s already has a RainMaker device", id);
        return ESP_OK;
    }

    char *priv_id = strdup(id);
    if (!priv_id) {
        ESP_LOGE(TAG, "Failed to strdup device id %s", id);
        return ESP_ERR_NO_MEM;
    }

    const char *rmaker_name = dev->name;
    esp_rmaker_device_t *rmaker_dev = NULL;

    switch (type) {
    case DEVICE_TYPE_ON_OFF:
        rmaker_dev = esp_rmaker_switch_device_create(rmaker_name, priv_id, false);
        break;

    case DEVICE_TYPE_DIMMABLE:
        rmaker_dev = esp_rmaker_lightbulb_device_create(rmaker_name, priv_id, false);
        break;

    case DEVICE_TYPE_TEMPERATURE_SENSOR: {
        rmaker_dev = esp_rmaker_temp_sensor_device_create(rmaker_name, priv_id, 0.0f);
        if (rmaker_dev) {
            esp_rmaker_param_t *hum = esp_rmaker_param_create("Humidity", NULL, esp_rmaker_float(0.0f), PROP_FLAG_READ);
            if (hum) esp_rmaker_device_add_param(rmaker_dev, hum);
        }
        break;
    }

    case DEVICE_TYPE_HUMIDITY_SENSOR: {
        rmaker_dev = esp_rmaker_device_create(rmaker_name, ESP_RMAKER_DEVICE_OTHER, priv_id);
        if (rmaker_dev) {
            esp_rmaker_param_t *hum = esp_rmaker_param_create("Humidity", NULL, esp_rmaker_float(0.0f), PROP_FLAG_READ);
            if (hum) esp_rmaker_device_add_param(rmaker_dev, hum);
        }
        break;
    }

    case DEVICE_TYPE_CONTACT_SENSOR: {
        rmaker_dev = esp_rmaker_device_create(rmaker_name, ESP_RMAKER_DEVICE_OTHER, priv_id);
        if (rmaker_dev) {
            esp_rmaker_param_t *contact = esp_rmaker_param_create("Contact", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
            if (contact) esp_rmaker_device_add_param(rmaker_dev, contact);
        }
        break;
    }

    case DEVICE_TYPE_OCCUPANCY_SENSOR: {
        rmaker_dev = esp_rmaker_device_create(rmaker_name, ESP_RMAKER_DEVICE_OTHER, priv_id);
        if (rmaker_dev) {
            esp_rmaker_param_t *occ = esp_rmaker_param_create("Occupancy", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
            if (occ) esp_rmaker_device_add_param(rmaker_dev, occ);
        }
        break;
    }

    case DEVICE_TYPE_LIGHT_SENSOR: {
        rmaker_dev = esp_rmaker_device_create(rmaker_name, ESP_RMAKER_DEVICE_OTHER, priv_id);
        if (rmaker_dev) {
            esp_rmaker_param_t *light = esp_rmaker_param_create("Light", NULL, esp_rmaker_int(0), PROP_FLAG_READ);
            if (light) esp_rmaker_device_add_param(rmaker_dev, light);
        }
        break;
    }

    case DEVICE_TYPE_TANQUE: {
        rmaker_dev = esp_rmaker_device_create(rmaker_name, ESP_RMAKER_DEVICE_OTHER, priv_id);
        if (rmaker_dev) {
            esp_rmaker_param_t *level = esp_rmaker_param_create("Level", NULL, esp_rmaker_int(0), PROP_FLAG_READ);
            if (level) esp_rmaker_device_add_param(rmaker_dev, level);
        }
        break;
    }

    case DEVICE_TYPE_GAS_SENSOR: {
        rmaker_dev = esp_rmaker_device_create(rmaker_name, ESP_RMAKER_DEVICE_OTHER, priv_id);
        if (rmaker_dev) {
            esp_rmaker_param_t *gas = esp_rmaker_param_create("GasLevel", NULL, esp_rmaker_int(0), PROP_FLAG_READ);
            if (gas) esp_rmaker_device_add_param(rmaker_dev, gas);
            esp_rmaker_param_t *alarm = esp_rmaker_param_create("GasAlarm", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
            if (alarm) esp_rmaker_device_add_param(rmaker_dev, alarm);
        }
        break;
    }

    default:
        ESP_LOGE(TAG, "Unsupported device type: %d", type);
        free(priv_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (!rmaker_dev) {
        ESP_LOGE(TAG, "Failed to create RainMaker device for %s", id);
        free(priv_id);
        return ESP_FAIL;
    }

    if (type == DEVICE_TYPE_ON_OFF || type == DEVICE_TYPE_DIMMABLE) {
        esp_rmaker_device_add_cb(rmaker_dev, write_cb, NULL);
    }

    esp_err_t err = esp_rmaker_node_add_device(s_node, rmaker_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device %s to node", id);
        esp_rmaker_device_delete(rmaker_dev);
        free(priv_id);
        return err;
    }

    device_registry_set_rmaker_handle(id, rmaker_dev);
    ESP_LOGI(TAG, "RainMaker device created: %s (%s)", rmaker_name, id);
    return ESP_OK;
}

esp_err_t rmaker_gateway_device_remove(const char *id)
{
    if (!s_initialized || !s_node || !id) return ESP_FAIL;

    void *rmaker_dev = device_registry_get_rmaker_handle(id);
    if (!rmaker_dev) {
        ESP_LOGW(TAG, "Device %s has no RainMaker handle", id);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = esp_rmaker_node_remove_device(s_node, (const esp_rmaker_device_t *)rmaker_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove device %s from node: %s", id, esp_err_to_name(err));
    }

    err = esp_rmaker_device_delete((const esp_rmaker_device_t *)rmaker_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to delete RainMaker device %s: %s", id, esp_err_to_name(err));
    }

    // Free the priv_data (strdup'd ID)
    bridged_device_t *dev = device_registry_get_by_id(id);
    if (dev) {
        dev->rmaker_device_hdl = NULL;
    }

    ESP_LOGI(TAG, "RainMaker device removed: %s", id);
    return ESP_OK;
}

esp_err_t rmaker_gateway_device_update_state(const char *id, const char *key, const char *value)
{
    if (!s_initialized || !id || !key || !value) return ESP_FAIL;

    void *rmaker_dev = device_registry_get_rmaker_handle(id);
    if (!rmaker_dev) {
        return ESP_ERR_NOT_FOUND;
    }

    const char *param_name = NULL;
    esp_rmaker_param_val_t param_val;
    param_val.type = RMAKER_VAL_TYPE_INVALID;

    if (strcmp(key, "on") == 0) {
        param_name = ESP_RMAKER_DEF_POWER_NAME;
        param_val = esp_rmaker_bool(strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "level") == 0) {
        param_name = ESP_RMAKER_DEF_BRIGHTNESS_NAME;
        param_val = esp_rmaker_int(atoi(value));
    } else if (strcmp(key, "temperature") == 0) {
        param_name = ESP_RMAKER_DEF_TEMPERATURE_NAME;
        param_val = esp_rmaker_float((float)atof(value));
    } else if (strcmp(key, "humidity") == 0) {
        param_name = "Humidity";
        param_val = esp_rmaker_float((float)atof(value));
    } else if (strcmp(key, "contact") == 0) {
        param_name = "Contact";
        param_val = esp_rmaker_bool(strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "occupancy") == 0) {
        param_name = "Occupancy";
        param_val = esp_rmaker_bool(strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "light_level") == 0) {
        param_name = "Light";
        param_val = esp_rmaker_int(atoi(value));
    } else if (strcmp(key, "gas_level") == 0) {
        param_name = "GasLevel";
        param_val = esp_rmaker_int(atoi(value));
    } else if (strcmp(key, "alarm") == 0) {
        param_name = "GasAlarm";
        param_val = esp_rmaker_bool(strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else {
        ESP_LOGW(TAG, "Unknown state key: %s for device %s", key, id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(
            (const esp_rmaker_device_t *)rmaker_dev, param_name);
    if (!param) {
        ESP_LOGW(TAG, "Param %s not found on device %s", param_name, id);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = esp_rmaker_param_update_and_report(param, param_val);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update param %s for device %s: %s",
                param_name, id, esp_err_to_name(err));
    }
    return err;
}
