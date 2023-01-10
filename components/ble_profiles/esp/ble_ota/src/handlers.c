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

static esp_ble_ota_char_t
find_ota_char_and_desr_by_handle(char *ep_name)
{
    for (int i = 0; i < ENDPOINTS; i++) {
        if (strcmp(ep_name, "recv-fw")) {
            return RECV_FW_CHAR;
        }
        if (strcmp(ep_name, "ota-bar")) {
            return OTA_STATUS_CHAR;
        }
        if (strcmp(ep_name, "ota-command")) {
            return CMD_CHAR;
        }
        if (strcmp(ep_name, "ota-customer")) {
            return CUS_CHAR;
        }
    }
    return INVALID_CHAR;
}

static esp_err_t
ota_send_file(uint8_t *file, size_t length)
{
    struct os_mbuf *om = ble_hs_mbuf_from_flat(file, length);
    ble_ota_write_chr(om);

    return ESP_OK;
}

static esp_err_t
ota_start_cmd(uint8_t *cmd, size_t length)
{
    ESP_LOGI(TAG, "%s len = %d data = ", __func__, length);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(cmd, length);
    ble_ota_start_write_chr(om);
    return ESP_OK;
}

static esp_err_t *
ota_subscribe(char *ep_name)
{
    ESP_LOGI(TAG, "%s %s", __func__, ep_name);

    esp_ble_ota_char_t ota_char;
    ota_char = find_ota_char_and_desr_by_handle(ep_name);

    esp_ble_ota_subscribe(ota_char);

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
    ptr->ota_subscribe = ota_subscribe;
    return ESP_OK;
}
