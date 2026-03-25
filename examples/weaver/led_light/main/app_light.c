/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_weaver.h>

#include "app_driver.h"
#include "app_light.h"

static const char *TAG = "app_light";

static esp_weaver_device_t *s_light_device = NULL;

/* Callback to handle param updates received from local control (bulk callback) */
static esp_err_t bulk_write_cb(const esp_weaver_device_t *device, const esp_weaver_param_write_req_t write_req[],
                               uint8_t count, void *priv_data, esp_weaver_write_ctx_t *ctx)
{
    const char *device_name = esp_weaver_device_get_name(device);
    ESP_LOGI(TAG, "Light received %d params in write", count);

    for (int i = 0; i < count; i++) {
        esp_weaver_param_t *param = write_req[i].param;
        esp_weaver_param_val_t val = write_req[i].val;
        const char *param_name = esp_weaver_param_get_name(param);

        if (strcmp(param_name, ESP_WEAVER_DEF_POWER_NAME) == 0) {
            ESP_LOGI(TAG, "%s.%s = %s", device_name, param_name, val.val.b ? "true" : "false");
            app_light_set_power(val.val.b);
        } else if (strcmp(param_name, ESP_WEAVER_DEF_BRIGHTNESS_NAME) == 0) {
            ESP_LOGI(TAG, "%s.%s = %d", device_name, param_name, val.val.i);
            app_light_set_brightness(val.val.i);
        } else if (strcmp(param_name, ESP_WEAVER_DEF_HUE_NAME) == 0) {
            ESP_LOGI(TAG, "%s.%s = %d", device_name, param_name, val.val.i);
            app_light_set_hue(val.val.i);
        } else if (strcmp(param_name, ESP_WEAVER_DEF_SATURATION_NAME) == 0) {
            ESP_LOGI(TAG, "%s.%s = %d", device_name, param_name, val.val.i);
            app_light_set_saturation(val.val.i);
        } else {
            ESP_LOGW(TAG, "Unknown parameter: %s", param_name);
            continue;
        }

        esp_weaver_param_update(param, val);
    }
    return ESP_OK;
}

esp_weaver_device_t *app_light_device_create(bool default_power,
                                             int default_brightness, int default_hue, int default_saturation)
{
    s_light_device = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    if (!s_light_device) {
        ESP_LOGE(TAG, "Could not create light device");
        return NULL;
    }

    esp_weaver_device_add_bulk_cb(s_light_device, bulk_write_cb);

    esp_weaver_param_t *p;

    p = esp_weaver_param_create(ESP_WEAVER_DEF_NAME_PARAM, ESP_WEAVER_PARAM_NAME,
                                esp_weaver_str("Light"), PROP_FLAG_READ);
    if (p) {
        esp_weaver_param_add_ui_type(p, ESP_WEAVER_UI_TEXT);
        esp_weaver_device_add_param(s_light_device, p);
    }

    p = esp_weaver_param_create(ESP_WEAVER_DEF_POWER_NAME, ESP_WEAVER_PARAM_POWER,
                                esp_weaver_bool(default_power), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    if (p) {
        esp_weaver_param_add_ui_type(p, ESP_WEAVER_UI_TOGGLE);
        esp_weaver_device_add_param(s_light_device, p);
        esp_weaver_device_assign_primary_param(s_light_device, p);
    }

    p = esp_weaver_param_create(ESP_WEAVER_DEF_BRIGHTNESS_NAME, ESP_WEAVER_PARAM_BRIGHTNESS,
                                esp_weaver_int(default_brightness), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    if (p) {
        esp_weaver_param_add_ui_type(p, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(p, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1));
        esp_weaver_device_add_param(s_light_device, p);
    }

    p = esp_weaver_param_create(ESP_WEAVER_DEF_HUE_NAME, ESP_WEAVER_PARAM_HUE,
                                esp_weaver_int(default_hue), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    if (p) {
        esp_weaver_param_add_ui_type(p, ESP_WEAVER_UI_HUE_SLIDER);
        esp_weaver_param_add_bounds(p, esp_weaver_int(0), esp_weaver_int(360), esp_weaver_int(1));
        esp_weaver_device_add_param(s_light_device, p);
    }

    p = esp_weaver_param_create(ESP_WEAVER_DEF_SATURATION_NAME, ESP_WEAVER_PARAM_SATURATION,
                                esp_weaver_int(default_saturation), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    if (p) {
        esp_weaver_param_add_ui_type(p, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(p, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1));
        esp_weaver_device_add_param(s_light_device, p);
    }

    return s_light_device;
}

esp_weaver_param_t *app_light_get_param_by_type(const char *type)
{
    if (!s_light_device) {
        return NULL;
    }
    return esp_weaver_device_get_param_by_type(s_light_device, type);
}
