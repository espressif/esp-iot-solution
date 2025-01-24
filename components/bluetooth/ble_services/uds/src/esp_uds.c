/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief User Data Service(UDS)
 */

#include <string.h>

#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_uds.h"

ESP_EVENT_DEFINE_BASE(BLE_UDS_EVENTS);

/* UDS database change increment characteristic value */
static esp_ble_uds_db_t s_db_chg_val = {
    .len = 0,
    .db_buf = {0}
};

/* UDS User Index characteristic value */
static uint8_t s_user_index = 0xff;

/* UDS User Control Point characteristic value */
static esp_ble_uds_user_ctrl_t s_user_ctrl = {
    .op_code = BLE_UDS_USER_CTRL_REG_NEW_OP,
    .param_len = 0,
    .parameter = {0}
};

static esp_ble_uds_reg_user_t reg_user_val = {
    .header = {0},
    .flag = {0},
    .user_index = 0,
    .user_name = "BLE_UDS"
};

esp_err_t esp_ble_uds_get_db_incre(esp_ble_uds_db_t *out_val)
{
    if (!out_val) {
        ESP_LOGE("ERROR", "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    out_val->len = s_db_chg_val.len;
    if (out_val->len != 0) {
        memcpy(out_val->db_buf, s_db_chg_val.db_buf, out_val->len);
    } else {
        memset(out_val->db_buf, 0x0, BLE_UDS_ATT_VAL_LEN);
    }

    return ESP_OK;
}

esp_err_t esp_ble_uds_set_db_incre(esp_ble_uds_db_t *in_val, bool need_send)
{
    if ((in_val == NULL) || (in_val->len == 0)) {
        ESP_LOGE("ERROR", "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    s_db_chg_val.len = MIN(BLE_UDS_ATT_VAL_LEN, in_val->len);
    memcpy(s_db_chg_val.db_buf, in_val->db_buf, s_db_chg_val.len);

    if (need_send) {
#ifdef CONFIG_BLE_UDS_FEATURE_DB_CHG_NTF_ENABLE
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_UDS_CHR_UUID16_DATABASH_CHG,
            },
            .data = s_db_chg_val.db_buf,
            .data_len = s_db_chg_val.len,
        };

        return esp_ble_conn_notify(&conn_data);
#else
        return ESP_ERR_INVALID_ARG;
#endif
    }

    return ESP_OK;
}

esp_err_t esp_ble_uds_get_user_ctrl(esp_ble_uds_user_ctrl_t *out_val)
{
    if (!out_val) {
        ESP_LOGE("ERROR", "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    out_val->op_code = s_user_ctrl.op_code;
    out_val->param_len = s_user_ctrl.param_len;

    if (out_val->param_len != 0) {
        memcpy(out_val->parameter, s_user_ctrl.parameter, out_val->param_len);
    } else {
        memset(out_val->parameter, 0x0, BLE_UDS_USER_CTRL_POINT_PARAM_LEN);
    }

    return ESP_OK;
}

esp_err_t esp_ble_uds_set_user_ctrl(esp_ble_uds_user_ctrl_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE("ERROR", "in_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t send_buf[BLE_UDS_USER_CTRL_POINT_PARAM_LEN + 1] = {0};

    s_user_ctrl.op_code = in_val->op_code;
    s_user_ctrl.param_len = MIN(BLE_UDS_USER_CTRL_POINT_PARAM_LEN, in_val->param_len);
    if (s_user_ctrl.param_len != 0) {
        memcpy(s_user_ctrl.parameter, in_val->parameter, s_user_ctrl.param_len);
    }

    if (need_send) {
        send_buf[0] = s_user_ctrl.op_code;
        if (s_user_ctrl.param_len != 0) {
            memcpy(send_buf + 1, s_user_ctrl.parameter, s_user_ctrl.param_len);
        }

        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_UDS_CHR_UUID16_USER_CTRL,
            },
            .data = send_buf,
            .data_len = s_user_ctrl.param_len + 1,
        };

        return esp_ble_conn_write(&conn_data);
    }

    return ESP_OK;
}

esp_err_t esp_ble_uds_get_reg_user(esp_ble_uds_reg_user_t *out_val)
{
    if (!out_val) {
        ESP_LOGE("ERROR", "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    out_val->header = reg_user_val.header;
    out_val->flag = reg_user_val.flag;
    out_val->user_index = reg_user_val.user_index;

    if (strlen((char *)(reg_user_val.user_name)) != 0) {
        memcpy(out_val->user_name, reg_user_val.user_name, strlen((char *)(reg_user_val.user_name)));
    } else {
        memset(out_val->user_name, 0x0, BLE_UDS_ATT_VAL_LEN);
    }

    return ESP_OK;
}

esp_err_t esp_ble_uds_set_reg_user(esp_ble_uds_reg_user_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE("ERROR", "in_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    reg_user_val.header = in_val->header;
    reg_user_val.flag = in_val->flag;
    reg_user_val.user_index = in_val->user_index;

    memcpy(reg_user_val.user_name, in_val->user_name, strlen((char *)(in_val->user_name)));

    if (need_send) {
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_UDS_CHR_UUID16_USER_CTRL,
            },
            .data = (uint8_t *)(&reg_user_val),
            .data_len = strlen((char *)(in_val->user_name)) + 3,
        };

        return esp_ble_conn_write(&conn_data);
    }

    return ESP_OK;
}

static esp_err_t uds_db_chg_cb(const uint8_t *inbuf, uint16_t inlen,
                               uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = sizeof(s_db_chg_val.db_buf);

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf) {
        s_db_chg_val.len = MIN(len, inlen);
        memcpy(s_db_chg_val.db_buf, inbuf, MIN(len, inlen));
        esp_event_post(BLE_UDS_EVENTS, BLE_UDS_CHR_UUID16_DATABASH_CHG, ((uint8_t *)(&s_db_chg_val)), sizeof(esp_ble_uds_db_t), portMAX_DELAY);
        return ESP_OK;
    }

    *outbuf = calloc(1, s_db_chg_val.len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, s_db_chg_val.db_buf, s_db_chg_val.len);
    *outlen = s_db_chg_val.len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static esp_err_t uds_user_index_cb(const uint8_t *inbuf, uint16_t inlen,
                                   uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(s_user_index);
    *outbuf = (uint8_t *)calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_user_index, *outlen);

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}

static esp_err_t uds_user_ctrl_cb(const uint8_t *inbuf, uint16_t inlen,
                                  uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_INTERNAL_ERROR;

    if (!outbuf || !outlen) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf) {
        if (inlen == 0) {
            return ESP_ERR_INVALID_ARG;
        }

        s_user_ctrl.op_code = inbuf[0];
        memcpy(s_user_ctrl.parameter, inbuf + 1, MIN(sizeof(s_user_ctrl.parameter), (inlen - 1)));
        esp_event_post(BLE_UDS_EVENTS, BLE_UDS_CHR_UUID16_USER_CTRL, ((uint8_t *)(&s_user_ctrl)), sizeof(esp_ble_uds_user_ctrl_t), portMAX_DELAY);
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "Database change Increment", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE
#ifdef CONFIG_BLE_UDS_FEATURE_DB_CHG_NTF_ENABLE
        | BLE_CONN_GATT_CHR_NOTIFY
#endif
        , { BLE_UDS_CHR_UUID16_DATABASH_CHG }, uds_db_chg_cb
    },

    {
        "User Index", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ
        , { BLE_UDS_CHR_UUID16_USER_INDEX }, uds_user_index_cb
    },

    {
        "User Control Point", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_INDICATE
        , { BLE_UDS_CHR_UUID16_USER_CTRL }, uds_user_ctrl_cb
    },

    {
        "Registered User", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_INDICATE
        , { BLE_UDS_CHR_UUID16_REG_USER }, NULL
    },
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_UDS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_uds_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
