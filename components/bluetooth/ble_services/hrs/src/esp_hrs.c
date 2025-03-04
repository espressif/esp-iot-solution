/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Heart Rate Service
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_hrs.h"

static uint8_t s_location = 0x01;
static esp_ble_hrs_hrm_t s_ble_hrs_hrm;

static esp_err_t ble_hrs_hrm_notify(esp_ble_hrs_hrm_t *hrm)
{
    uint16_t len = 0;
    uint8_t  hrs_data[20] = { 0 };

    memcpy(hrs_data, &hrm->flags, sizeof(hrm->flags));
    len += sizeof(hrm->flags);

    if (hrm->flags.format) {
        memcpy(&hrs_data[len], &hrm->heartrate.u16, sizeof(hrm->heartrate.u16));
        len += sizeof(hrm->heartrate.u16);
    } else {
        memcpy(&hrs_data[len], &hrm->heartrate.u8, sizeof(hrm->heartrate.u8));
        len += sizeof(hrm->heartrate.u8);
    }

    if (hrm->flags.energy) {
        memcpy(&hrs_data[len], &hrm->energy_val, sizeof(hrm->energy_val));
        len += sizeof(hrm->energy_val);
    }

    if (hrm->flags.interval) {
        memcpy(&hrs_data[len], &hrm->interval_buf, sizeof(hrm->interval_buf));
        len += sizeof(hrm->interval_buf);
    }

    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_HRS_CHR_UUID16_MEASUREMENT,
        },
        .data = hrs_data,
        .data_len = len,
    };

    return esp_ble_conn_notify(&conn_data);
}

#ifdef CONFIG_BLE_HRS_BODY_SENSOR_LOCATION
static esp_err_t esp_hrs_body_sensor_cb(const uint8_t *inbuf, uint16_t inlen,
                                        uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t len = sizeof(s_location);
    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_location, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_HRS_CONTROL_POINT
static esp_err_t esp_hrs_ctrl_cb(const uint8_t *inbuf, uint16_t inlen,
                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t cmd_id = 0;

    if (!inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    cmd_id = inbuf[0];
    switch (cmd_id) {
    case 0x01:
        s_ble_hrs_hrm.energy_val = 0;
        ble_hrs_hrm_notify(&s_ble_hrs_hrm);
        break;
    default:
        break;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

esp_err_t esp_ble_hrs_get_hrm(esp_ble_hrs_hrm_t *hrm)
{
    if (!hrm) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(hrm, &s_ble_hrs_hrm, sizeof(esp_ble_hrs_hrm_t));

    return ESP_OK;
}

esp_err_t esp_ble_hrs_set_hrm(esp_ble_hrs_hrm_t *hrm)
{
    if (!hrm) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_hrs_hrm, hrm, sizeof(esp_ble_hrs_hrm_t));

    return ble_hrs_hrm_notify(&s_ble_hrs_hrm);
}

esp_err_t esp_ble_hrs_get_location(uint8_t *location)
{
    if (!location) {
        return ESP_ERR_INVALID_ARG;
    }

    *location = s_location;

    return ESP_OK;
}

esp_err_t esp_ble_hrs_set_location(uint8_t location)
{
    if (location >= BLE_HRS_CHR_BODY_SENSOR_LOC_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (location != s_location) {
        s_location = location;
    }

    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {"measurement",     BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_NOTIFY, { BLE_HRS_CHR_UUID16_MEASUREMENT }, NULL},
#ifdef CONFIG_BLE_HRS_BODY_SENSOR_LOCATION
    {"body_sensor",     BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ, { BLE_HRS_CHR_UUID16_BODY_SENSOR_LOC }, esp_hrs_body_sensor_cb},
#endif
#ifdef CONFIG_BLE_HRS_CONTROL_POINT
    {"ctrl",            BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_WRITE, { BLE_HRS_CHR_UUID16_HEART_RATE_CNTL_POINT }, esp_hrs_ctrl_cb},
#endif
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_HRS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_hrs_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
