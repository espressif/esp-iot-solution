/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Alert Notification Profile
 */

#include <string.h>

#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_anp.h"

static const char* TAG = "esp_anp";

/* Supported categories bitmasks */
static uint8_t ble_svc_ans_new_alert_cat;
static uint8_t ble_svc_ans_unr_alert_cat;

/* Characteristic values */
static uint8_t ble_svc_ans_new_alert_val[BLE_ANP_NEW_ALERT_MAX_LEN];
static uint8_t ble_svc_ans_unr_alert_stat[2];
static uint8_t ble_svc_ans_alert_not_ctrl_pt[2];

/* Alert counts, one value for each category */
static uint8_t ble_svc_ans_new_alert_cnt[BLE_ANP_CAT_NUM];
static uint8_t ble_svc_ans_unr_alert_cnt[BLE_ANP_CAT_NUM];

ESP_EVENT_DEFINE_BASE(BLE_ANP_EVENTS);

//4.09 Configure Alert Notification Control Point
static esp_err_t esp_ble_anp_alert_ctrl_set(uint8_t cmd_id, uint8_t cat_id)
{
    /* ANS Control point command and category variables */
    uint8_t cat_bit_mask;

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ANP_CHR_UUID16_ALERT_NOT_CTRL_PT,
        },
        .data = ble_svc_ans_alert_not_ctrl_pt,
        .data_len = sizeof(ble_svc_ans_alert_not_ctrl_pt),
    };

    ble_svc_ans_alert_not_ctrl_pt[0] = cmd_id;
    ble_svc_ans_alert_not_ctrl_pt[1] = cat_id;

    /* Get command ID and category ID */
    cmd_id = ble_svc_ans_alert_not_ctrl_pt[0];
    cat_id = ble_svc_ans_alert_not_ctrl_pt[1];

    /* Set cat_bit_mask to the appropriate bitmask based on cat_id */
    if (cat_id < BLE_ANP_CAT_NUM) {
        cat_bit_mask = (1 << cat_id);
    } else if (cat_id == 0xFF) {
        cat_bit_mask = cat_id;
    } else {
        /* invalid category ID */
        return ESP_ERR_INVALID_ARG;
    }

    switch (cmd_id) {
    case BLE_ANP_CMD_EN_NEW_ALERT_CAT:
        ble_svc_ans_new_alert_cat |= cat_bit_mask;
        break;
    case BLE_ANP_CMD_EN_UNR_ALERT_CAT:
        ble_svc_ans_unr_alert_cat |= cat_bit_mask;
        break;
    case BLE_ANP_CMD_DIS_NEW_ALERT_CAT:
        ble_svc_ans_new_alert_cat &= ~cat_bit_mask;
        break;
    case BLE_ANP_CMD_DIS_UNR_ALERT_CAT:
        ble_svc_ans_unr_alert_cat &= ~cat_bit_mask;
        break;
    case BLE_ANP_CMD_NOT_NEW_ALERT_IMMEDIATE:
        if (cat_id == 0xFF) {
            /* If cat_id is 0xFF, notify on all enabled categories */
            for (int i = BLE_ANP_CAT_NUM - 1; i > 0; --i) {
                if ((ble_svc_ans_new_alert_cat >> i) & 0x01) {
                    ESP_LOGI(TAG, "Request the New Alert in the server to notify immediately on all enabled categories %d", i);
                }
            }
        } else {
            ESP_LOGI(TAG, "Request the New Alert in the server to notify immediately on current enabled categories %d", cat_id);
        }
        break;
    case BLE_ANP_CMD_NOT_UNR_ALERT_IMMEDIATE:
        if (cat_id == 0xFF) {
            /* If cat_id is 0xFF, notify on all enabled categories */
            for (int i = BLE_ANP_CAT_NUM - 1; i > 0; --i) {
                if ((ble_svc_ans_unr_alert_cat >> i) & 0x01) {
                    ESP_LOGI(TAG, "Request the Unread Alert Status in the server to notify immediately on all enabled categories %d", i);
                }
            }
        } else {
            ESP_LOGI(TAG, "Request the Unread Alert Status in the server to notify immediately on current enabled categories %d", cat_id);
        }
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t rc = esp_ble_conn_write(&inbuff);
    if (rc == 0) {
        ESP_LOGI(TAG, "Configure Alert Notification Control Point Success!");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, inbuff.data, inbuff.data_len, ESP_LOG_INFO);
    }

    return rc;
}

static esp_err_t esp_ble_anp_handle_new_alert(const uint8_t *inbuf, uint16_t inlen)
{
    if (!inbuf || inlen < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cat_id = inbuf[0];
    uint8_t cat_bit_mask;
    const char *cat_info = (inlen > 2) ? (const char *)(inbuf + (inlen - 2)) : NULL;

    if (cat_id < BLE_ANP_CAT_NUM) {
        cat_bit_mask = (1 << cat_id);
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if ((cat_bit_mask & ble_svc_ans_new_alert_cat) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 4.5 Receive Notification of New Alert */
    ble_svc_ans_new_alert_cnt[cat_id] = inbuf[1];

    /* Set ID and count values */
    ble_svc_ans_new_alert_val[0] = cat_id;
    ble_svc_ans_new_alert_val[1] = ble_svc_ans_new_alert_cnt[cat_id];

    ESP_LOGI(TAG, "Receive Notification of New Alert on cat_id %d which count change to %d", ble_svc_ans_new_alert_val[0], ble_svc_ans_new_alert_val[1]);

    if (cat_info) {
        /* If cat_info is longer than the max string length only write up to the maximum length */
        memcpy(&ble_svc_ans_new_alert_val[2], cat_info, MIN(strlen(cat_info), BLE_ANP_INFO_STR_MAX_LEN));
    }

    return ESP_OK;
}

//4.7 Receive Notification of Unread Alert Status characteristic
static esp_err_t esp_ble_anp_handle_unr_alert(const uint8_t *inbuf, uint16_t inlen)
{
    if (!inbuf || inlen != 2) {
        ESP_LOGE(TAG, "Receive Notification of Unread Alert Status Must include category ID and Status");
        return ESP_FAIL;
    }

    uint8_t cat_id = inbuf[0];
    uint8_t cat_bit_mask;

    if (cat_id < BLE_ANP_CAT_NUM) {
        cat_bit_mask = 1 << cat_id;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if ((cat_bit_mask & ble_svc_ans_unr_alert_cat) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_svc_ans_unr_alert_cnt[cat_id] = inbuf[1];

    ble_svc_ans_unr_alert_stat[0] = cat_id;
    ble_svc_ans_unr_alert_stat[1] = ble_svc_ans_unr_alert_cnt[cat_id];

    ESP_LOGI(TAG, "Receive Notification of Unread Alert Status on cat_id %d which status change to %d", ble_svc_ans_unr_alert_stat[0], ble_svc_ans_unr_alert_stat[1]);

    return ESP_OK;;
}

static void esp_ble_anp_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DATA_RECEIVE\n");
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        esp_ble_anp_data_t   ble_anp_data;
        memset(&ble_anp_data, 0, sizeof(esp_ble_anp_data_t));
        switch (conn_data->uuid.uuid16) {
        case BLE_ANP_CHR_UUID16_NEW_ALERT:
            esp_ble_anp_handle_new_alert(conn_data->data, conn_data->data_len);
            memcpy(&ble_anp_data.new_alert_val, ble_svc_ans_new_alert_val, sizeof(ble_svc_ans_new_alert_val));
            esp_event_post(BLE_ANP_EVENTS, BLE_ANP_CHR_UUID16_NEW_ALERT, &ble_anp_data, sizeof(esp_ble_anp_data_t), portMAX_DELAY);
            break;
        case BLE_ANP_CHR_UUID16_UNR_ALERT_STAT:
            esp_ble_anp_handle_unr_alert(conn_data->data, conn_data->data_len);
            memcpy(&ble_anp_data.unr_alert_stat, ble_svc_ans_unr_alert_stat, sizeof(ble_svc_ans_unr_alert_stat));
            esp_event_post(BLE_ANP_EVENTS, BLE_ANP_CHR_UUID16_UNR_ALERT_STAT, &ble_anp_data, sizeof(esp_ble_anp_data_t), portMAX_DELAY);
            break;
        default:
            break;
        }

        break;
    default:
        break;
    }
}

esp_err_t esp_ble_anp_get_new_alert(uint8_t cat_id, uint8_t *cat_val)
{
    if (!cat_val) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ANP_CHR_UUID16_SUP_NEW_ALERT_CAT,
        },
        .data = NULL,
        .data_len = 0,
    };

    esp_err_t rc = esp_ble_conn_read(&inbuff);
    if (rc) {
        ESP_LOGE(TAG, "Read the Value of Supported New Alert Category %s!", esp_err_to_name(rc));
        return rc;
    }

    ble_svc_ans_new_alert_cat = *inbuff.data;

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, inbuff.data, inbuff.data_len, ESP_LOG_INFO);

    if (cat_id != 0xFF) {
        if (cat_id > BLE_ANP_CAT_NUM) {
            ESP_LOGE(TAG, "Invalid Category ID %d to Check Supported New Alert Category!", cat_id);
            return ESP_ERR_INVALID_ARG;
        }

        // 4.12 Check Supported New Alert Category
        *cat_val = ((ble_svc_ans_new_alert_cat >> cat_id) & 0x01);
    } else {
        // 4.03 Read the Value of Supported New Alert Category
        *cat_val = ble_svc_ans_new_alert_cat;
    }

    return ESP_OK;
}

esp_err_t esp_ble_anp_set_new_alert(uint8_t cat_id, esp_ble_anp_option_t option)
{
    esp_err_t rc = ESP_ERR_INVALID_ARG;
    if (cat_id > BLE_ANP_CAT_NUM && cat_id != 0xFF) {
        ESP_LOGE(TAG, "Invalid Category ID %d to Request or Recovery New Alert!", cat_id);
        return ESP_ERR_INVALID_ARG;
    }

    switch (option) {
    case BLE_ANP_OPT_ENABLE:
        if (cat_id != 0xFF) {
            ble_svc_ans_new_alert_cnt[cat_id] += 1;
        }
        rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_EN_NEW_ALERT_CAT, cat_id);
        if (!rc) {
            rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_NOT_NEW_ALERT_IMMEDIATE, cat_id);
        }
        break;
    case BLE_ANP_OPT_DISABLE:
        rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_DIS_NEW_ALERT_CAT, cat_id);
        break;
    case BLE_ANP_OPT_RECOVER:
        rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_EN_NEW_ALERT_CAT, cat_id);
        if (!rc) {
            rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_NOT_NEW_ALERT_IMMEDIATE, 0xFF);
        }
        break;
    default:
        break;
    }

    return rc;
}

esp_err_t esp_ble_anp_get_unr_alert(uint8_t cat_id, uint8_t *cat_val)
{
    if (!cat_val) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ANP_CHR_UUID16_SUP_UNR_ALERT_CAT,
        },
        .data = NULL,
        .data_len = 0,
    };

    esp_err_t rc = esp_ble_conn_read(&inbuff);
    if (rc) {
        ESP_LOGE(TAG, "Read the Value of Supported Unread Alert Category %s!", esp_err_to_name(rc));
        return rc;
    }

    ble_svc_ans_unr_alert_cat = *inbuff.data;

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &ble_svc_ans_unr_alert_cat, sizeof(ble_svc_ans_unr_alert_cat), ESP_LOG_INFO);

    if (cat_id != 0xFF) {
        if (cat_id > BLE_ANP_CAT_NUM) {
            ESP_LOGE(TAG, "Invalid Category ID %d to Check Supported Unread Alert Status Category!", cat_id);
            return ESP_ERR_INVALID_ARG;
        }

        /* 4.13 Check Supported Unread Alert Status Category */
        *cat_val = ((ble_svc_ans_unr_alert_cat >> cat_id) & 0x01);
    } else {
        /* 4.4 Read the Value of Supported Unread Alert Status Category */
        *cat_val = ble_svc_ans_unr_alert_cat;
    }

    return ESP_OK;
}

esp_err_t esp_ble_anp_set_unr_alert(uint8_t cat_id, esp_ble_anp_option_t option)
{
    esp_err_t rc = ESP_ERR_INVALID_ARG;
    if (cat_id > BLE_ANP_CAT_NUM && cat_id != 0xFF) {
        ESP_LOGE(TAG, "Invalid Category ID %d to Request or Recovery Unread Alert Status!", cat_id);
        return ESP_ERR_INVALID_ARG;
    }

    switch (option) {
    case BLE_ANP_OPT_ENABLE:
        if (cat_id != 0xFF) {
            ble_svc_ans_unr_alert_cnt[cat_id] += 1;
        }
        rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_EN_UNR_ALERT_CAT, cat_id);
        if (!rc) {
            rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_NOT_UNR_ALERT_IMMEDIATE, cat_id);
        }
        break;
    case BLE_ANP_OPT_DISABLE:
        rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_DIS_UNR_ALERT_CAT, cat_id);
        break;
    case BLE_ANP_OPT_RECOVER:
        rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_EN_UNR_ALERT_CAT, cat_id);
        if (!rc) {
            rc = esp_ble_anp_alert_ctrl_set(BLE_ANP_CMD_NOT_UNR_ALERT_IMMEDIATE, 0xFF);
        }
        break;
    default:
        break;
    }

    return rc;
}

esp_err_t esp_ble_anp_init(void)
{
    return esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, esp_ble_anp_event, NULL);
}

esp_err_t esp_ble_anp_deinit(void)
{
    return esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, esp_ble_anp_event);
}
