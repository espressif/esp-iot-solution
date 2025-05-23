/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Current Time Service(CTS)
 */

#include <string.h>

#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_cts.h"

ESP_EVENT_DEFINE_BASE(BLE_CTS_EVENTS);

/* CTS Current Time Characteristic Value */
static esp_ble_cts_cur_time_t s_cur_time;

/* CTS Local Time Characteristic Value */
static esp_ble_cts_local_time_t s_local_time;

/* CTS Reference Time Characteristic Value */
static esp_ble_cts_ref_time_t s_ref_time;

esp_err_t esp_ble_cts_get_current_time(esp_ble_cts_cur_time_t *out_val)
{
    if (!out_val) {
        ESP_LOGE("ERROR", "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(out_val, &s_cur_time, sizeof(esp_ble_cts_cur_time_t));

    return ESP_OK;
}

esp_err_t esp_ble_cts_set_current_time(esp_ble_cts_cur_time_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE("ERROR", "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_cur_time, in_val, sizeof(esp_ble_cts_cur_time_t));

    if (need_send) {
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_CTS_CHR_UUID16_CURRENT_TIME,
            },
            .data = (uint8_t *)(&s_cur_time),
            .data_len = sizeof(s_cur_time),
        };

        return esp_ble_conn_notify(&conn_data);
    }

    return ESP_OK;
}

esp_err_t esp_ble_cts_get_local_time(esp_ble_cts_local_time_t *out_val)
{
    if (!out_val) {
        ESP_LOGE("ERROR", "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(out_val, &s_local_time, sizeof(esp_ble_cts_local_time_t));

    return ESP_OK;
}

esp_err_t esp_ble_cts_set_local_time(esp_ble_cts_local_time_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE("ERROR", "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_local_time, in_val, sizeof(esp_ble_cts_local_time_t));

    return ESP_OK;
}

esp_err_t esp_ble_cts_get_reference_time(esp_ble_cts_ref_time_t *out_val)
{
    if (!out_val) {
        ESP_LOGE("ERROR", "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(out_val, &s_ref_time, sizeof(esp_ble_cts_ref_time_t));

    return ESP_OK;
}

esp_err_t esp_ble_cts_set_reference_time(esp_ble_cts_ref_time_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE("ERROR", "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ref_time, in_val, sizeof(esp_ble_cts_ref_time_t));

    return ESP_OK;
}

#ifdef CONFIG_BLE_CTS_REF_TIME_CHAR_ENABLE
static esp_err_t cts_ref_time_cb(const uint8_t *inbuf, uint16_t inlen,
                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = sizeof(s_ref_time);

    memcpy(&s_ref_time, inbuf, MIN(len, inlen));
    esp_event_post(BLE_CTS_EVENTS, BLE_CTS_CHR_UUID16_REFERENCE_TIME, ((uint8_t *)(&s_ref_time)), sizeof(esp_ble_cts_ref_time_t), portMAX_DELAY);

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_CTS_LOCAL_TIME_CHAR_ENABLE
static esp_err_t cts_local_time_cb(const uint8_t *inbuf, uint16_t inlen,
                                   uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = sizeof(s_local_time);

#ifdef CONFIG_BLE_CTS_LOCAL_TIME_WRITE_ENABLE
    if (inbuf) {
        memcpy(&s_local_time, inbuf, MIN(len, inlen));
        esp_event_post(BLE_CTS_EVENTS, BLE_CTS_CHR_UUID16_LOCAL_TIME, ((uint8_t *)(&s_local_time)), sizeof(esp_ble_cts_local_time_t), portMAX_DELAY);
        return ESP_OK;
    }
#endif

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_local_time, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

static esp_err_t cts_cur_time_cb(const uint8_t *inbuf, uint16_t inlen,
                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = sizeof(s_cur_time);

#ifdef CONFIG_BLE_CTS_CURRENT_TIME_WRITE_ENABLE
    if (inbuf) {
        memcpy(&s_cur_time, inbuf, MIN(len, inlen));
        esp_event_post(BLE_CTS_EVENTS, BLE_CTS_CHR_UUID16_CURRENT_TIME, ((uint8_t *)(&s_cur_time)), sizeof(esp_ble_cts_cur_time_t), portMAX_DELAY);
        return ESP_OK;
    }
#endif

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_cur_time, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "Current Time", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY
#ifdef CONFIG_BLE_CTS_CURRENT_TIME_WRITE_ENABLE
        | BLE_CONN_GATT_CHR_WRITE
#endif
        , { BLE_CTS_CHR_UUID16_CURRENT_TIME }, cts_cur_time_cb
    },
#ifdef CONFIG_BLE_CTS_LOCAL_TIME_CHAR_ENABLE
    {
        "Local Time", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ
#ifdef CONFIG_BLE_CTS_LOCAL_TIME_WRITE_ENABLE
        | BLE_CONN_GATT_CHR_WRITE
#endif
        , { BLE_CTS_CHR_UUID16_LOCAL_TIME }, cts_local_time_cb
    },
#endif
#ifdef CONFIG_BLE_CTS_REF_TIME_CHAR_ENABLE
    {
        "Reference Time", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_WRITE
        , { BLE_CTS_CHR_UUID16_REFERENCE_TIME }, cts_ref_time_cb
    },
#endif
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_CTS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_cts_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
