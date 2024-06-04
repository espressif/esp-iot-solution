/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "settings.h"

static const char *TAG = "settings";

#define NAME_SPACE "sys_param"
#define KEY "param"

static sys_param_t g_sys_param = {0};

static const sys_param_t g_default_sys_param = {
    .report_type = TINYUSB_HID_REPORT,
};

static esp_err_t settings_check(sys_param_t *param)
{
    esp_err_t ret;
    ESP_GOTO_ON_FALSE(param->report_type < BTN_REPORT_TYPE_MAX, ESP_ERR_INVALID_ARG, reset, TAG, "report_type incorrect");
    return ret;
reset:
    ESP_LOGW(TAG, "Set to default");
    memcpy(&g_sys_param, &g_default_sys_param, sizeof(sys_param_t));
    return ret;
}

esp_err_t settings_read_parameter_from_nvs(void)
{
    nvs_handle_t my_handle = 0;
    esp_err_t ret = nvs_open(NAME_SPACE, NVS_READONLY, &my_handle);
    if (ESP_ERR_NVS_NOT_FOUND == ret) {
        ESP_LOGW(TAG, "Not found, Set to default");
        memcpy(&g_sys_param, &g_default_sys_param, sizeof(sys_param_t));
        settings_write_parameter_to_nvs();
        return ESP_OK;
    }

    ESP_GOTO_ON_FALSE(ESP_OK == ret, ret, err, TAG, "nvs open failed (0x%x)", ret);

    size_t len = sizeof(sys_param_t);
    ret = nvs_get_blob(my_handle, KEY, &g_sys_param, &len);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ret, err, TAG, "can't read param");
    nvs_close(my_handle);

    settings_check(&g_sys_param);
    return ret;
err:
    if (my_handle) {
        nvs_close(my_handle);
    }
    return ret;
}

esp_err_t settings_write_parameter_to_nvs(void)
{
    ESP_LOGI(TAG, "Saving settings");
    settings_check(&g_sys_param);
    nvs_handle_t my_handle = {0};
    esp_err_t err = nvs_open(NAME_SPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        err = nvs_set_blob(my_handle, KEY, &g_sys_param, sizeof(sys_param_t));
        err |= nvs_commit(my_handle);
        nvs_close(my_handle);
    }
    return ESP_OK == err ? ESP_OK : ESP_FAIL;
}

sys_param_t *settings_get_parameter(void)
{
    return &g_sys_param;
}
