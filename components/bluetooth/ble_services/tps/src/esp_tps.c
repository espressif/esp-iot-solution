/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief TX Power Service
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_tps.h"

static int8_t s_ble_tps_tx_power_level;

static esp_err_t esp_tps_tx_power_level_cb(const uint8_t *inbuf, uint16_t inlen,
                                           uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(s_ble_tps_tx_power_level);
    *outbuf = (uint8_t *)calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }
    memcpy(*outbuf, &s_ble_tps_tx_power_level, *outlen);

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

int8_t esp_ble_tps_get_tx_power_level(void)
{
    return s_ble_tps_tx_power_level;
}

esp_err_t esp_ble_tps_set_tx_power_level(int8_t tx_power_level)
{
    s_ble_tps_tx_power_level = tx_power_level;

    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {"tx_power_level",     BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ, { BLE_TPS_CHR_UUID16_TX_POWER_LEVEL }, esp_tps_tx_power_level_cb},
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_TPS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_tps_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
