/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Object Transfer Service(OTS)
 */

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ots.h"

ESP_EVENT_DEFINE_BASE(BLE_OTS_EVENTS);

static const char *TAG = "OTS";

/* Mutex to protect global variables */
static SemaphoreHandle_t s_ots_mutex = NULL;
#define OTS_MUTEX_TIMEOUT_MS 2000
#define OTS_MUTEX_TIMEOUT_TICKS pdMS_TO_TICKS(OTS_MUTEX_TIMEOUT_MS)

/* Helper macro to take mutex with timeout and error handling */
#define OTS_MUTEX_TAKE() do { \
    if (s_ots_mutex != NULL) { \
        if (xSemaphoreTake(s_ots_mutex, OTS_MUTEX_TIMEOUT_TICKS) != pdTRUE) { \
            ESP_LOGW(TAG, "Failed to take mutex, timeout after %d ms", OTS_MUTEX_TIMEOUT_MS); \
        } \
    } \
} while (0)

/* Helper macro to give mutex */
#define OTS_MUTEX_GIVE() do { \
    if (s_ots_mutex != NULL) { \
        xSemaphoreGive(s_ots_mutex); \
    } \
} while (0)

/* OTS feature Characteristic Value */
static esp_ble_ots_feature_t s_ots_feature_val;

/* Object Name Characteristic value */
static uint8_t s_obj_name[BLE_OTS_ATT_VAL_LEN] = {0};

/* Object Type Characteristic value */
static uint16_t s_obj_type = 0xFFFF;

/* Object Size Characteristic value */
static esp_ble_ots_size_t s_obj_size = {
    .allocated_size = 0,
    .current_size = 0
};

/* Object First Create Time */
static esp_ble_ots_utc_t s_obj_first_create;

/* Object Last Modify Time */
static esp_ble_ots_utc_t s_obj_last_modify;

/* Object ID */
static esp_ble_ots_id_t s_obj_id;

/* Object Properties */
static esp_ble_ots_prop_t s_obj_prop;

/* Object Action Control Point */
static esp_ble_ots_oacp_t s_obj_oacp;

/* Object List Control Point */
static esp_ble_ots_olcp_t s_obj_olcp;

/* Object Filter */
static esp_ble_ots_filter_t s_obj_filter;

/* Object Change */
static esp_ble_ots_change_t s_obj_change;

esp_err_t esp_ble_ots_get_feature(esp_ble_ots_feature_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_ots_feature_val, sizeof(esp_ble_ots_feature_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_feature(esp_ble_ots_feature_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_ots_feature_val, in_val, sizeof(esp_ble_ots_feature_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_name(uint8_t *out_buf, size_t buf_len, size_t *out_len)
{
    if (!out_buf || buf_len == 0) {
        ESP_LOGE(TAG, "out_buf is NULL or buf_len is zero.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    size_t actual_len = strnlen((const char *)s_obj_name, BLE_OTS_ATT_VAL_LEN);
    size_t copy_len = (actual_len < buf_len) ? actual_len : buf_len - 1;

    memcpy(out_buf, s_obj_name, copy_len);
    out_buf[copy_len] = '\0';

    if (out_len) {
        if (actual_len >= buf_len) {
            /* Truncation occurred: set out_len to the number of bytes actually copied */
            *out_len = copy_len;
        } else {
            /* No truncation: set out_len to the actual length */
            *out_len = actual_len;
        }
    }
    OTS_MUTEX_GIVE();

    /* Return ESP_ERR_INVALID_SIZE if truncation occurred, ESP_OK otherwise */
    return (actual_len >= buf_len) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

esp_err_t esp_ble_ots_set_name(const uint8_t *in_val, size_t in_len)
{
    if (in_val == NULL || in_len == 0) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    size_t copy_len = (in_len < BLE_OTS_ATT_VAL_LEN) ? in_len : BLE_OTS_ATT_VAL_LEN - 1;
    memcpy(s_obj_name, in_val, copy_len);
    s_obj_name[copy_len] = '\0';
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_type(uint16_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    *out_val = s_obj_type;
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_type(uint16_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    s_obj_type = *in_val;
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_size(esp_ble_ots_size_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_size, sizeof(s_obj_size));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_size(esp_ble_ots_size_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_size, in_val, sizeof(s_obj_size));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_first_create_time(esp_ble_ots_utc_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_first_create, sizeof(esp_ble_ots_utc_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_first_create_time(esp_ble_ots_utc_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_first_create, in_val, sizeof(esp_ble_ots_utc_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_last_modify_time(esp_ble_ots_utc_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_last_modify, sizeof(esp_ble_ots_utc_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_last_modify_time(esp_ble_ots_utc_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_last_modify, in_val, sizeof(esp_ble_ots_utc_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_id(esp_ble_ots_id_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_id, sizeof(esp_ble_ots_id_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_id(esp_ble_ots_id_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_id, in_val, sizeof(esp_ble_ots_id_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_prop(esp_ble_ots_prop_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_prop, sizeof(esp_ble_ots_prop_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_prop(esp_ble_ots_prop_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_prop, in_val, sizeof(esp_ble_ots_prop_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_oacp(esp_ble_ots_oacp_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_oacp, sizeof(esp_ble_ots_oacp_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_oacp(esp_ble_ots_oacp_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_oacp, in_val, sizeof(esp_ble_ots_oacp_t));
    esp_ble_ots_oacp_t oacp_copy = s_obj_oacp;
    OTS_MUTEX_GIVE();

    if (need_send) {
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT,
            },
            .data = (uint8_t *)(&oacp_copy),
            .data_len = sizeof(esp_ble_ots_oacp_t),
        };

        return esp_ble_conn_write(&conn_data);
    }

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_olcp(esp_ble_ots_olcp_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_olcp, sizeof(esp_ble_ots_olcp_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_olcp(esp_ble_ots_olcp_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_olcp, in_val, sizeof(esp_ble_ots_olcp_t));
    esp_ble_ots_olcp_t olcp_copy = s_obj_olcp;
    OTS_MUTEX_GIVE();

    if (need_send) {
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT,
            },
            .data = (uint8_t *)(&olcp_copy),
            .data_len = sizeof(esp_ble_ots_olcp_t),
        };

        return esp_ble_conn_write(&conn_data);
    }

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_filter(esp_ble_ots_filter_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_filter, sizeof(esp_ble_ots_filter_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_filter(esp_ble_ots_filter_t *in_val)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_filter, in_val, sizeof(esp_ble_ots_filter_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_get_change(esp_ble_ots_change_t *out_val)
{
    if (!out_val) {
        ESP_LOGE(TAG, "out_val is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(out_val, &s_obj_change, sizeof(esp_ble_ots_change_t));
    OTS_MUTEX_GIVE();

    return ESP_OK;
}

esp_err_t esp_ble_ots_set_change(esp_ble_ots_change_t *in_val, bool need_send)
{
    if (in_val == NULL) {
        ESP_LOGE(TAG, "Invalid parameter.");
        return ESP_ERR_INVALID_ARG;
    }

    OTS_MUTEX_TAKE();
    memcpy(&s_obj_change, in_val, sizeof(esp_ble_ots_change_t));
    esp_ble_ots_change_t change_copy = s_obj_change;
    OTS_MUTEX_GIVE();

    if (need_send) {
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_OTS_CHR_UUID16_OBJECT_CHANGED,
            },
            .data = (uint8_t *)(&change_copy),
            .data_len = sizeof(esp_ble_ots_change_t),
        };

        return esp_ble_conn_write(&conn_data);
    }

    return ESP_OK;
}

static esp_err_t ots_feature_cb(const uint8_t *inbuf, uint16_t inlen,
                                uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(esp_ble_ots_feature_t);
    *outbuf = calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_ots_feature_val, *outlen);
    OTS_MUTEX_GIVE();

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t ots_object_name_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf) {
        /* Write operation: copy input data and ensure null termination */
        uint8_t name_copy[BLE_OTS_ATT_VAL_LEN];
        size_t copy_len = (inlen < BLE_OTS_ATT_VAL_LEN) ? inlen : BLE_OTS_ATT_VAL_LEN - 1;
        OTS_MUTEX_TAKE();
        memcpy(s_obj_name, inbuf, copy_len);
        s_obj_name[copy_len] = '\0';
        memcpy(name_copy, s_obj_name, copy_len + 1);
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_NAME, name_copy, copy_len + 1, pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    /* Allocate buffer with actual string length including null terminator */
    OTS_MUTEX_TAKE();
    size_t actual_len = strlen((const char *)s_obj_name);
    size_t alloc_len = actual_len + 1;

    *outbuf = calloc(1, alloc_len);
    if (!(*outbuf)) {
        OTS_MUTEX_GIVE();
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, s_obj_name, alloc_len);
    OTS_MUTEX_GIVE();
    *outlen = alloc_len;
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t ots_object_type_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(s_obj_type);
    *outbuf = calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_type, *outlen);
    OTS_MUTEX_GIVE();

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t ots_object_size_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(s_obj_size);
    *outbuf = calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_size, *outlen);
    OTS_MUTEX_GIVE();

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

#ifdef CONFIG_BLE_OTS_FIRST_CREATED_CHARACTERISTIC_ENABLE
static esp_err_t ots_object_first_create_cb(const uint8_t *inbuf, uint16_t inlen,
                                            uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t len = sizeof(esp_ble_ots_utc_t);

    if (inbuf) {
        esp_ble_ots_utc_t first_create_copy;
        OTS_MUTEX_TAKE();
        memcpy(&s_obj_first_create, inbuf, MIN(len, inlen));
        memcpy(&first_create_copy, &s_obj_first_create, sizeof(esp_ble_ots_utc_t));
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_FIRST_CREATED, &first_create_copy, sizeof(esp_ble_ots_utc_t), pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_first_create, len);
    OTS_MUTEX_GIVE();
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_OTS_LAST_MODIFIED_CHARACTERISTIC_ENABLE
static esp_err_t ots_object_last_modify_cb(const uint8_t *inbuf, uint16_t inlen,
                                           uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t len = sizeof(esp_ble_ots_utc_t);

    if (inbuf) {
        esp_ble_ots_utc_t last_modify_copy;
        OTS_MUTEX_TAKE();
        memcpy(&s_obj_last_modify, inbuf, MIN(len, inlen));
        memcpy(&last_modify_copy, &s_obj_last_modify, sizeof(esp_ble_ots_utc_t));
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_LAST_MODIFIED, &last_modify_copy, sizeof(esp_ble_ots_utc_t), pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_last_modify, len);
    OTS_MUTEX_GIVE();
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}
#endif

static esp_err_t ots_object_id_cb(const uint8_t *inbuf, uint16_t inlen,
                                  uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(esp_ble_ots_id_t);
    *outbuf = calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_id, *outlen);
    OTS_MUTEX_GIVE();

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t ots_object_prop_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t len = sizeof(esp_ble_ots_prop_t);

    if (inbuf) {
        esp_ble_ots_prop_t prop_copy;
        OTS_MUTEX_TAKE();
        memcpy(&s_obj_prop, inbuf, MIN(len, inlen));
        memcpy(&prop_copy, &s_obj_prop, sizeof(esp_ble_ots_prop_t));
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_PROP, &prop_copy, sizeof(esp_ble_ots_prop_t), pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_prop, len);
    OTS_MUTEX_GIVE();
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t ots_object_oacp_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t len = sizeof(esp_ble_ots_oacp_t);

    if (inbuf) {
        esp_ble_ots_oacp_t oacp_copy;
        OTS_MUTEX_TAKE();
        memcpy(&s_obj_oacp, inbuf, MIN(len, inlen));
        memcpy(&oacp_copy, &s_obj_oacp, sizeof(esp_ble_ots_oacp_t));
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, &oacp_copy, sizeof(esp_ble_ots_oacp_t), pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    *att_status = ESP_IOT_ATT_INVALID_PDU;
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t ots_object_olcp_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t len = sizeof(esp_ble_ots_olcp_t);

    if (inbuf) {
        esp_ble_ots_olcp_t olcp_copy;
        OTS_MUTEX_TAKE();
        memcpy(&s_obj_olcp, inbuf, MIN(len, inlen));
        memcpy(&olcp_copy, &s_obj_olcp, sizeof(esp_ble_ots_olcp_t));
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, &olcp_copy, sizeof(esp_ble_ots_olcp_t), pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    *att_status = ESP_IOT_ATT_INVALID_PDU;
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t ots_object_filter_cb(const uint8_t *inbuf, uint16_t inlen,
                                      uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t len = sizeof(esp_ble_ots_filter_t);

    if (inbuf) {
        esp_ble_ots_filter_t filter_copy;
        OTS_MUTEX_TAKE();
        memcpy(&s_obj_filter, inbuf, MIN(len, inlen));
        memcpy(&filter_copy, &s_obj_filter, sizeof(esp_ble_ots_filter_t));
        OTS_MUTEX_GIVE();
        esp_event_post(BLE_OTS_EVENTS, BLE_OTS_CHR_UUID16_OBJECT_LIST_FILTER, &filter_copy, sizeof(esp_ble_ots_filter_t), pdMS_TO_TICKS(100));
        *att_status = ESP_IOT_ATT_SUCCESS;
        return ESP_OK;
    }

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    OTS_MUTEX_TAKE();
    memcpy(*outbuf, &s_obj_filter, len);
    OTS_MUTEX_GIVE();
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "OTS Feature", BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ
        , { BLE_OTS_CHR_UUID16_OTS_FEATURE }, ots_feature_cb
    },

    {
        "Object Name", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE
        , { BLE_OTS_CHR_UUID16_OBJECT_NAME }, ots_object_name_cb
    },

    {
        "Object Type", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ
        , { BLE_OTS_CHR_UUID16_OBJECT_TYPE }, ots_object_type_cb
    },

    {
        "Object Size", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ
        , { BLE_OTS_CHR_UUID16_OBJECT_SIZE }, ots_object_size_cb
    },

#ifdef CONFIG_BLE_OTS_FIRST_CREATED_CHARACTERISTIC_ENABLE
    {
        "Object First Created", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE
        , { BLE_OTS_CHR_UUID16_OBJECT_FIRST_CREATED }, ots_object_first_create_cb
    },
#endif

#ifdef CONFIG_BLE_OTS_LAST_MODIFIED_CHARACTERISTIC_ENABLE
    {
        "Object Last Modified", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE
        , { BLE_OTS_CHR_UUID16_OBJECT_LAST_MODIFIED }, ots_object_last_modify_cb
    },
#endif

    {
        "Object ID", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ
        , { BLE_OTS_CHR_UUID16_OBJECT_ID }, ots_object_id_cb
    },

    {
        "Object Properties", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE
        , { BLE_OTS_CHR_UUID16_OBJECT_PROP }, ots_object_prop_cb
    },

    {
        "Object Action Control Point", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_INDICATE
        , { BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT }, ots_object_oacp_cb
    },

    {
        "Object List Control Point", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_INDICATE
        , { BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT }, ots_object_olcp_cb
    },

    {
        "Object List Filter", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE
        , { BLE_OTS_CHR_UUID16_OBJECT_LIST_FILTER }, ots_object_filter_cb
    },

#ifdef CONFIG_BLE_OTS_OBJECT_CHANGE_CHARACTERISTIC_ENABLE
    {
        "Object Changed", BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_INDICATE
        , { BLE_OTS_CHR_UUID16_OBJECT_CHANGED }, NULL
    },
#endif
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_OTS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_ots_init(void)
{
    if (s_ots_mutex == NULL) {
        s_ots_mutex = xSemaphoreCreateMutex();
        if (s_ots_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    return esp_ble_conn_add_svc(&svc);
}

esp_err_t esp_ble_ots_deinit(void)
{
    esp_err_t ret = esp_ble_conn_remove_svc(&svc);

    if (s_ots_mutex != NULL) {
        /* Wait for any pending operations to complete */
        OTS_MUTEX_TAKE();
        OTS_MUTEX_GIVE();
        vSemaphoreDelete(s_ots_mutex);
        s_ots_mutex = NULL;
    }

    return ret;
}
