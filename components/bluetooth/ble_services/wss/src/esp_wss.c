/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Weight Scale Service(WSS)
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_wss.h"

#define BLE_WSS_MAX_VAL_LEN                             20

typedef struct {
    uint8_t len;
    uint8_t buf[BLE_WSS_MAX_VAL_LEN];
} indicate_buf_t;

/* Weight Scale Feature value */
static esp_ble_wss_feature_t s_wss_feature = {
    .timestamp = 1,
    .user_id = 1,
    .bmi = 1,
    .weight = 1,
    .w_resolution = 1,
    .height = 1,
    .h_resolution = 1
};

static esp_ble_wss_measurement_t s_wss_measurement;

static void build_wss_ind_buf(esp_ble_wss_measurement_t *in_val, indicate_buf_t *out_buf)
{
    uint32_t offset = 0;

    /* Add Flag */
    memcpy(out_buf->buf + offset, &(in_val->flag), sizeof(uint32_t));
    offset += sizeof(uint32_t);

    /* Add weight */
    memcpy(out_buf->buf + offset, &(in_val->weight), sizeof(uint16_t));
    offset += sizeof(uint32_t);

    if (in_val->flag.time_present == 1) {
        memcpy(out_buf->buf + offset, &(in_val->timestamp), sizeof(in_val->timestamp));
        offset += sizeof(in_val->timestamp); // timestamp length
    }

    if (in_val->flag.user_present == 1) {
        memcpy(out_buf->buf + offset, &(in_val->user_id), sizeof(uint8_t));
        offset += sizeof(uint8_t); // user id length
    }

    if (in_val->flag.bmi_height_present == 1) {
        memcpy(out_buf->buf + offset, &(in_val->bmi), sizeof(uint8_t));
        offset += sizeof(uint8_t); // bmi length

        memcpy(out_buf->buf + offset, &(in_val->height), sizeof(uint16_t));
        offset += sizeof(uint16_t); // height length
    }

    out_buf->len = offset;

    return;
}

esp_err_t esp_ble_wss_get_measurement(esp_ble_wss_measurement_t *out_val)
{
    if (!out_val) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(out_val, &s_wss_measurement, sizeof(esp_ble_wss_measurement_t));

    return ESP_OK;
}

esp_err_t esp_ble_wss_set_measurement(esp_ble_wss_measurement_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_wss_measurement, in_val, sizeof(esp_ble_wss_measurement_t));

    /* Set Weight Scale Feature Bit Masks */
    memset(&s_wss_feature, 0x0, sizeof(esp_ble_wss_feature_t));
    s_wss_feature.weight = 1;

    if (in_val->flag.measurement_unit == 0x1) {
        s_wss_feature.w_resolution = in_val->weight_resolution;
        s_wss_feature.h_resolution = in_val->height_resolution;
    }

    if (in_val->flag.time_present == 0x1) {
        s_wss_feature.timestamp = 1;
    }

    if (in_val->flag.user_present == 0x1) {
        s_wss_feature.user_id = 1;
    }

    if (in_val->flag.bmi_height_present == 0x1) {
        s_wss_feature.bmi = 1;
        s_wss_feature.height = 1;
    }

    indicate_buf_t out_buf;

    /* Build Body Composition Measurement value */
    build_wss_ind_buf(in_val, &out_buf);

    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_WSS_CHR_UUID16_WEIGHT_MEASUREMENT,
        },
        .data = out_buf.buf,
        .data_len = out_buf.len,
    };

    return esp_ble_conn_write(&conn_data);
}

static esp_err_t wss_feature_cb(const uint8_t *inbuf, uint16_t inlen,
                                uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = sizeof(s_wss_feature);

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_wss_feature, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "weight feature", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ
        , { BLE_WSS_CHR_UUID16_WEIGHT_FEATURE }, wss_feature_cb
    },
    {
        "weight measurement", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_INDICATE
        , { BLE_WSS_CHR_UUID16_WEIGHT_MEASUREMENT }, NULL
    },
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_WSS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_wss_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
