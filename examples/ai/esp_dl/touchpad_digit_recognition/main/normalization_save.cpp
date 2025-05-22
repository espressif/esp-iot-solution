/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "touch_digit.h"

#define NAME_SPACE "normalization"
#define KEY "data"

static const char *TAG = "normalization_save";

esp_err_t set_normalization_data(touch_digit_data_t *data)
{
    ESP_LOGI(TAG, "Saving Normalization data");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NAME_SPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        err = nvs_set_blob(my_handle, KEY, data, sizeof(touch_digit_data_t));
        err |= nvs_commit(my_handle);
        nvs_close(my_handle);
    }
    return ESP_OK == err ? ESP_OK : ESP_FAIL;
}

esp_err_t get_normalization_data(touch_digit_data_t *data)
{
    nvs_handle_t my_handle;
    size_t len = sizeof(touch_digit_data_t);
    esp_err_t ret = nvs_open(NAME_SPACE, NVS_READWRITE, &my_handle);
    if (ESP_ERR_NVS_NOT_FOUND == ret) {
        ESP_LOGW(TAG, "Not found, Set to default");
        touch_digit_data_t _data = {};
        set_normalization_data(&_data);
        *data = _data;
        return ESP_OK;
    }
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ret, err, TAG, "nvs open failed (0x%x)", ret);

    ret = nvs_get_blob(my_handle, KEY, data, &len);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ret, err, TAG, "can't read param");
    nvs_close(my_handle);

    return ret;
err:
    if (my_handle) {
        nvs_close(my_handle);
    }
    return ret;
}
