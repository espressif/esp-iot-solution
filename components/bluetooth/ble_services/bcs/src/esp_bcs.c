/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Body Composition Service(BCS)
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_bcs.h"
#include "esp_log.h"

typedef struct {
    uint8_t len;
    uint8_t buf[BLE_BCS_MAX_VAL_LEN];
} indicate_buf_t;

static uint32_t body_composition_feature = BLE_BCS_FEAT_TIME_STAMP |
                                           BLE_BCS_FEAT_MULTI_USER |
                                           BLE_BCS_FEAT_BASAL_METABOLISM |
                                           BLE_BCS_FEAT_MUSCLE_PERCENTAGE |
                                           BLE_BCS_FEAT_MUSCLE_MASS |
                                           BLE_BCS_FEAT_FAT_FREE_MASS |
                                           BLE_BCS_FEAT_SOFT_LEAN_MASS |
                                           BLE_BCS_FEAT_BODY_WATER_MASS |
                                           BLE_BCS_FEAT_IMPEDENCE |
                                           BLE_BCS_FEAT_WEIGHT |
                                           BLE_BCS_FEAT_HEIGHT |
                                           BLE_BCS_FEAT_MASS_MEASUREMENT_RESOLUTION |
                                           BLE_BCS_FEAT_HEIGHT_RESOLUTION;

static void build_bcs_ind_buf(esp_bcs_val_t *in_val, indicate_buf_t *out_buf)
{
    uint32_t offset = 0;

    /* Add Flag */
    memcpy(out_buf->buf + offset, &(in_val->bcs_flag), sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (in_val->bcs_flag | BLE_BCS_FLAG_TIME_STAMP) {
        memcpy(out_buf->buf + offset, &(in_val->timestamp), sizeof(in_val->timestamp));
        offset += sizeof(in_val->timestamp); // timestamp length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_MULTI_USER) {
        memcpy(out_buf->buf + offset, &(in_val->user_id), sizeof(uint8_t));
        offset += sizeof(uint8_t); // user id length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_BASAL_METABOLISM) {
        memcpy(out_buf->buf + offset, &(in_val->basal_metabolism), sizeof(uint16_t));
        offset += sizeof(uint16_t); // Basal Metabolism length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_MUSCLE_PERCENTAGE) {
        memcpy(out_buf->buf + offset, &(in_val->muscle_percentage), sizeof(uint16_t));
        offset += sizeof(uint16_t); // Muscle Percentage length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_MUSCLE_MASS) {
        memcpy(out_buf->buf + offset, &(in_val->muscle_mass), sizeof(uint16_t));
        offset += sizeof(uint16_t); // Muscle Mass length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_FAT_FREE_MASS) {
        memcpy(out_buf->buf + offset, &(in_val->fat_free_mass), sizeof(uint16_t));
        offset += sizeof(uint16_t); // Fat Free Mass length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_SOFT_LEAN_MASS) {
        memcpy(out_buf->buf + offset, &(in_val->soft_lean_mass), sizeof(uint16_t));
        offset += sizeof(uint16_t); // Soft Lean Mass length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_BODY_WATER_MASS) {
        memcpy(out_buf->buf + offset, &(in_val->body_water_mass), sizeof(uint16_t));
        offset += sizeof(uint16_t); // Body water Mass length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_IMPEDENCE) {
        memcpy(out_buf->buf + offset, &(in_val->impedance), sizeof(uint8_t));
        offset += sizeof(uint8_t); // impedance length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_WEIGHT) {
        memcpy(out_buf->buf + offset, &(in_val->weight), sizeof(uint16_t));
        offset += sizeof(uint16_t); // weight length
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_HEIGHT) {
        memcpy(out_buf->buf + offset, &(in_val->height), sizeof(uint16_t));
        offset += sizeof(uint16_t); // height length
    }

    out_buf->len = offset;

    return;
}

esp_err_t esp_ble_bcs_set_measurement(esp_bcs_val_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE("ERROR", "in_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    /* Set Body Composition Feature Bit Masks */
    body_composition_feature = 0;

    if (in_val->bcs_flag | BLE_BCS_FLAG_TIME_STAMP) {
        body_composition_feature |= BLE_BCS_FEAT_TIME_STAMP;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_MULTI_USER) {
        body_composition_feature |= BLE_BCS_FEAT_MULTI_USER;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_BASAL_METABOLISM) {
        body_composition_feature |= BLE_BCS_FEAT_BASAL_METABOLISM;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_MUSCLE_PERCENTAGE) {
        body_composition_feature |= BLE_BCS_FEAT_MUSCLE_PERCENTAGE;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_MUSCLE_MASS) {
        body_composition_feature |= BLE_BCS_FEAT_MUSCLE_MASS;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_FAT_FREE_MASS) {
        body_composition_feature |= BLE_BCS_FEAT_FAT_FREE_MASS;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_SOFT_LEAN_MASS) {
        body_composition_feature |= BLE_BCS_FEAT_SOFT_LEAN_MASS;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_BODY_WATER_MASS) {
        body_composition_feature |= BLE_BCS_FEAT_BODY_WATER_MASS;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_IMPEDENCE) {
        body_composition_feature |= BLE_BCS_FEAT_IMPEDENCE;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_WEIGHT) {
        body_composition_feature |= BLE_BCS_FEAT_WEIGHT;
    }

    if (in_val->bcs_flag | BLE_BCS_FLAG_HEIGHT) {
        body_composition_feature |= BLE_BCS_FEAT_HEIGHT;
    }

#ifdef CONFIG_BLE_BCS_FEATURE_INDICATE_ENABLE
    if (need_send) {
        indicate_buf_t out_buf;

        /* Build Body Composition Measurement value */
        build_bcs_ind_buf(in_val, &out_buf);

        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_BCS_CHR_UUID16_MEASUREMENT,
            },
            .data = out_buf.buf,
            .data_len = out_buf.len,
        };

        return esp_ble_conn_write(&conn_data);
    }
#endif

    return ESP_OK;
}

static esp_err_t bcs_feature_cb(const uint8_t *inbuf, uint16_t inlen,
                                uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(body_composition_feature);
    *outbuf = (uint8_t *)calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }
    memcpy(*outbuf, &body_composition_feature, *outlen);

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "feature", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ
#ifdef CONFIG_BLE_BCS_FEATURE_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
        , { BLE_BCS_CHR_UUID16_FEATURE }, bcs_feature_cb
    },

    {
        "measurement", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_INDICATE
        , { BLE_BCS_CHR_UUID16_MEASUREMENT }, NULL
    },
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_BCS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_bcs_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
