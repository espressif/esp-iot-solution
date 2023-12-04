/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>

#include "host/ble_hs.h"

#include "manager.h"
#include "esp_ble_ota_priv.h"

#include "ota.h"
#include "ble_ota.h"

#define ENDPOINTS 4

static const char *TAG = "ota_handlers";

static esp_err_t
ota_send_file(uint8_t *file, size_t length)
{
    esp_ble_ota_write(file, length);
    return ESP_OK;
}

static esp_err_t
ota_start_cmd(size_t file_size, size_t block_size, char *partition_name)
{
    /* Currently by default partition is OTA, so not setting it */

    ESP_LOGI(TAG, "%s file len = %d block size = %d", __func__, file_size, block_size);
    esp_ble_ota_set_sizes(file_size, block_size);
    return ESP_OK;
}

static esp_err_t
ota_finish_cmd()
{
    esp_ble_ota_finish();
    return ESP_OK;
}

esp_err_t
get_ota_handlers(ota_handlers_t *ptr)
{
    if (!ptr) {
        ESP_LOGE(TAG, "Null ptr");
        return ESP_ERR_INVALID_ARG;
    }
    ptr->ota_send_file = ota_send_file;
    ptr->ota_start_cmd = ota_start_cmd;
    ptr->ota_finish_cmd = ota_finish_cmd;
    return ESP_OK;
}
