/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Immediate Alert Service
 */

#include "esp_ble_conn_mgr.h"
#include "esp_ias.h"

/* Service state */
static uint8_t s_alert_level = ESP_BLE_IAS_ALERT_LEVEL_NO_ALERT;
static esp_ble_ias_alert_cb_t s_alert_cb;
static void *s_alert_cb_priv;

esp_err_t esp_ble_ias_register_alert_cb(esp_ble_ias_alert_cb_t cb, void *priv)
{
    s_alert_cb = cb;
    s_alert_cb_priv = priv;
    return ESP_OK;
}

esp_err_t esp_ble_ias_get_alert_level(uint8_t *level)
{
    if (!level) {
        return ESP_ERR_INVALID_ARG;
    }
    *level = s_alert_level;
    return ESP_OK;
}

static esp_err_t esp_ias_alert_level_cb(const uint8_t *inbuf, uint16_t inlen,
                                        uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)priv_data;
    (void)outbuf;
    (void)outlen;

    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!inbuf) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    if (inlen != 1) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t v = inbuf[0];
    if (v > ESP_BLE_IAS_ALERT_LEVEL_HIGH_ALERT) {
        *att_status = ESP_IOT_ATT_VALUE_NOT_ALLOWED;
        return ESP_ERR_INVALID_ARG;
    }

    s_alert_level = v;
    if (s_alert_cb) {
        s_alert_cb(s_alert_level, s_alert_cb_priv);
    }

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "alert_level",
        BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_WRITE_NO_RSP,
        { BLE_IAS_CHR_UUID16_ALERT_LEVEL },
        esp_ias_alert_level_cb,
    },
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_IAS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table,
};

esp_err_t esp_ble_ias_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
