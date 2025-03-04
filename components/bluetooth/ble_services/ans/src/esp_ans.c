/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Alert Notification Service
 */

#include <string.h>

#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ans.h"

static const char* TAG = "esp_ans";

/* Supported categories bitmasks */
static uint8_t ble_ans_new_alert_cat;
static uint8_t ble_ans_unr_alert_cat;

/* Characteristic values */
static uint8_t ble_ans_new_alert_val[BLE_ANS_NEW_ALERT_MAX_LEN];
static uint8_t ble_ans_unr_alert_stat[2];
static uint8_t ble_ans_alert_not_ctrl_pt[2];

/* Alert counts, one value for each category */
static uint8_t ble_ans_new_alert_cnt[BLE_ANS_CAT_NUM];
static uint8_t ble_ans_unr_alert_cnt[BLE_ANS_CAT_NUM];

/* Notify new alert */
static esp_err_t ble_ans_new_alert_notify(uint8_t cat_id, const char *cat_info)
{
    /* If cat_info is longer than the max string length only write up to the maximum length */
    uint8_t cat_info_len = cat_info ? MIN(strlen(cat_info), BLE_ANS_INFO_STR_MAX_LEN) : 0;
    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ANS_CHR_UUID16_NEW_ALERT,
        },
        .data = ble_ans_new_alert_val,
        .data_len = cat_info_len + 2,
    };

    /* Clear notification to remove old information that may persist */
    memset(&ble_ans_new_alert_val, 0, BLE_ANS_NEW_ALERT_MAX_LEN);

    /* Set ID and count values */
    ble_ans_new_alert_val[0] = cat_id;
    ble_ans_new_alert_val[1] = ble_ans_new_alert_cnt[cat_id];

    if (cat_info) {
        memcpy(&ble_ans_new_alert_val[2], cat_info, cat_info_len);
    }

    return esp_ble_conn_notify(&conn_data);
}

/* Notify unread alert */
static esp_err_t ble_ans_unr_alert_notify(uint8_t cat_id)
{
    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ANS_CHR_UUID16_UNR_ALERT_STAT,
        },
        .data = ble_ans_unr_alert_stat,
        .data_len = sizeof(ble_ans_unr_alert_stat),
    };

    ble_ans_unr_alert_stat[0] = cat_id;
    ble_ans_unr_alert_stat[1] = ble_ans_unr_alert_cnt[cat_id];

    return esp_ble_conn_notify(&conn_data);
}

static esp_err_t esp_ans_sup_new_alert_cat_cb(const uint8_t *inbuf, uint16_t inlen,
                                              uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = 0;
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    len = sizeof(ble_ans_new_alert_cat);
    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &ble_ans_new_alert_cat, len);
    *outlen = len;
    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static esp_err_t esp_ans_new_alert_cb(const uint8_t *inbuf, uint16_t inlen,
                                      uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = MIN(inlen, sizeof(ble_ans_new_alert_val));
    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf) {
        memcpy(ble_ans_new_alert_val, inbuf, len);
    } else {
        len = sizeof(ble_ans_new_alert_val);
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, ble_ans_new_alert_val, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static esp_err_t esp_ans_sup_unr_alert_cat_cb(const uint8_t *inbuf, uint16_t inlen,
                                              uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = 0;
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    len = sizeof(ble_ans_unr_alert_cat);
    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &ble_ans_unr_alert_cat, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static esp_err_t esp_ans_unr_alert_stat_cb(const uint8_t *inbuf, uint16_t inlen,
                                           uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = MIN(inlen, sizeof(ble_ans_unr_alert_stat));
    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf) {
        memcpy(ble_ans_unr_alert_stat, inbuf, len);
    } else {
        len = sizeof(ble_ans_unr_alert_stat);
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, ble_ans_unr_alert_stat, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static esp_err_t esp_ans_alert_notify_ctrl_cb(const uint8_t *inbuf, uint16_t inlen,
                                              uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    /* ANS Control point command and category variables */
    uint8_t cmd_id;
    uint8_t cat_id;
    uint8_t cat_bit_mask;
    uint8_t len = MIN(inlen, sizeof(ble_ans_alert_not_ctrl_pt));
    if (!inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(ble_ans_alert_not_ctrl_pt, inbuf, len);

    /* Get command ID and category ID */
    cmd_id = ble_ans_alert_not_ctrl_pt[0];
    cat_id = ble_ans_alert_not_ctrl_pt[1];

    /* Set cat_bit_mask to the appropriate bitmask based on cat_id */
    if (cat_id < BLE_ANS_CAT_NUM) {
        cat_bit_mask = (1 << cat_id);
    } else if (cat_id == 0xff) {
        cat_bit_mask = cat_id;
    } else {
        /* invalid category ID */
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    switch (cmd_id) {
    case BLE_ANS_CMD_EN_NEW_ALERT_CAT:
        ble_ans_new_alert_cat |= cat_bit_mask;
        break;
    case BLE_ANS_CMD_EN_UNR_ALERT_CAT:
        ble_ans_unr_alert_cat |= cat_bit_mask;
        break;
    case BLE_ANS_CMD_DIS_NEW_ALERT_CAT:
        ble_ans_new_alert_cat &= ~cat_bit_mask;
        break;
    case BLE_ANS_CMD_DIS_UNR_ALERT_CAT:
        ble_ans_unr_alert_cat &= ~cat_bit_mask;
        break;
    case BLE_ANS_CMD_NOT_NEW_ALERT_IMMEDIATE:
        if (cat_id == 0xff) {
            /* If cat_id is 0xff, notify on all enabled categories */
            for (int i = BLE_ANS_CAT_NUM - 1; i > 0; --i) {
                if ((ble_ans_new_alert_cat >> i) & 0x01) {
                    ble_ans_new_alert_notify(i, NULL);
                }
            }
        } else {
            ble_ans_new_alert_notify(cat_id, NULL);
        }
        break;
    case BLE_ANS_CMD_NOT_UNR_ALERT_IMMEDIATE:
        if (cat_id == 0xff) {
            /* If cat_id is 0xff, notify on all enabled categories */
            for (int i = BLE_ANS_CAT_NUM - 1; i > 0; --i) {
                if ((ble_ans_unr_alert_cat >> i) & 0x01) {
                    ble_ans_unr_alert_notify(i);
                }
            }
        } else {
            ble_ans_unr_alert_notify(cat_id);
        }
        break;
    default:
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

esp_err_t esp_ble_ans_get_unread_alert(uint8_t cat_id, uint8_t *cat_val)
{
    if (!cat_val) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cat_id != 0xFF) {
        if (cat_id > BLE_ANS_CAT_NUM) {
            ESP_LOGE(TAG, "Invalid Category ID %d to Check Supported Unread Alert Status Category!", cat_id);
            return ESP_ERR_INVALID_ARG;
        }

        /* Check Supported Unread Alert Status Category */
        *cat_val = ((ble_ans_unr_alert_cat >> cat_id) & 0x01);
    } else {
        /* Read the Value of Supported Unread Alert Status Category */
        *cat_val = ble_ans_unr_alert_cat;
    }

    return ESP_OK;
}

esp_err_t esp_ble_ans_set_unread_alert(uint8_t cat_id)
{
    uint8_t cat_bit_mask;

    if (cat_id < BLE_ANS_CAT_NUM) {
        cat_bit_mask = 1 << cat_id;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if ((cat_bit_mask & ble_ans_unr_alert_cat) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_ans_unr_alert_cnt[cat_id] += 1;
    return ble_ans_unr_alert_notify(cat_id);
}

esp_err_t esp_ble_ans_get_new_alert(uint8_t cat_id, uint8_t *cat_val)
{
    if (!cat_val) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cat_id != 0xFF) {
        if (cat_id > BLE_ANS_CAT_NUM) {
            ESP_LOGE(TAG, "Invalid Category ID %d to Check Supported New Alert Category!", cat_id);
            return ESP_ERR_INVALID_ARG;
        }

        /* Check Supported New Alert Category */
        *cat_val = ((ble_ans_new_alert_cat >> cat_id) & 0x01);
    } else {
        /* Read the Value of Supported New Alert Category */
        *cat_val = ble_ans_new_alert_cat;
    }

    return ESP_OK;
}

esp_err_t esp_ble_ans_set_new_alert(uint8_t cat_id, const char *cat_info)
{
    uint8_t cat_bit_mask;

    if (cat_id < BLE_ANS_CAT_NUM) {
        cat_bit_mask = (1 << cat_id);
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if ((cat_bit_mask & ble_ans_new_alert_cat) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_ans_new_alert_cnt[cat_id] += 1;
    return ble_ans_new_alert_notify(cat_id, cat_info);
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {"sup_new_alert_cat", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ,   { BLE_ANS_CHR_UUID16_SUP_NEW_ALERT_CAT},    esp_ans_sup_new_alert_cat_cb},
    {"new_alert",         BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_NOTIFY, { BLE_ANS_CHR_UUID16_NEW_ALERT },           esp_ans_new_alert_cb},
    {"sup_unr_alert_cat", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ,   { BLE_ANS_CHR_UUID16_SUP_UNR_ALERT_CAT},    esp_ans_sup_unr_alert_cat_cb},
    {"unr_alert_stat",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_NOTIFY, { BLE_ANS_CHR_UUID16_UNR_ALERT_STAT},       esp_ans_unr_alert_stat_cb},
    {"alert_notify_ctrl", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_WRITE,  { BLE_ANS_CHR_UUID16_ALERT_NOT_CTRL_PT },   esp_ans_alert_notify_ctrl_cb},
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_ANS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_ans_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
