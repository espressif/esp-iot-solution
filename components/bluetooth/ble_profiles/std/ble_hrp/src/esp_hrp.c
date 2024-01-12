/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Heart Rate Profile
 */

#include <string.h>

#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_hrp.h"

static const char* TAG = "esp_hrp";

ESP_EVENT_DEFINE_BASE(BLE_HRP_EVENTS);

static esp_ble_hrp_data_t s_ble_hrp_data;

static esp_err_t esp_ble_hrp_get_data(esp_ble_hrp_data_t *hrp_data, const uint8_t *inbuf, uint16_t inlen)
{
    if (!inbuf) {
        return ESP_ERR_INVALID_ARG;
    }

    /* The Flags Field */
    memcpy(&hrp_data->flags, inbuf, sizeof(hrp_data->flags));
    inbuf += sizeof(hrp_data->flags);

    /* Temperature Measurement Value Field */
    if (hrp_data->flags.format) {
        memcpy(&hrp_data->heartrate.u16, inbuf, sizeof(hrp_data->heartrate.u16));
        inbuf += sizeof(hrp_data->heartrate.u16);
    } else {
        memcpy(&hrp_data->heartrate.u8, inbuf, sizeof(hrp_data->heartrate.u8));
        inbuf += sizeof(hrp_data->heartrate.u8);
    }

    /* Sensor Contact Status Field */
    if (!hrp_data->flags.supported && hrp_data->flags.detected) {
        ESP_LOGW(TAG, "Skin Contact should be detected on Sensor Contact Feature Supported");
    } else {
        ESP_LOGW(TAG, "Sensor Contact Feature %s Supported", hrp_data->flags.supported ? "is" : "isn't");
        ESP_LOGW(TAG, "Skin Contact %s Detected", hrp_data->flags.detected ? "is" : "isn't");
    }

    /* Energy Expended Field */
    if (hrp_data->flags.energy) {
        memcpy(&hrp_data->energy_val, inbuf, sizeof(hrp_data->energy_val));
        inbuf += sizeof(hrp_data->energy_val);
    }

    /* RR-interval Field */
    if (hrp_data->flags.interval) {
        memcpy(&hrp_data->interval_buf, inbuf, sizeof(hrp_data->interval_buf));
    }

    return ESP_OK;
}

static void esp_ble_hrp_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DATA_RECEIVE\n");
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        memset(&s_ble_hrp_data, 0, sizeof(esp_ble_hrp_data_t));
        esp_ble_hrp_get_data(&s_ble_hrp_data, conn_data->data, conn_data->data_len);
        esp_event_post(BLE_HRP_EVENTS, BLE_HRP_CHR_UUID16_MEASUREMENT, &s_ble_hrp_data, sizeof(esp_ble_hrp_data_t), portMAX_DELAY);
        break;
    default:
        break;
    }
}

esp_err_t esp_ble_hrp_get_location(uint8_t *location)
{
    if (!location || !s_ble_hrp_data.flags.supported || s_ble_hrp_data.flags.detected) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_HRP_CHR_UUID16_BODY_SENSOR_LOC,
        },
        .data = NULL,
        .data_len = 0,
    };

    esp_err_t rc = esp_ble_conn_read(&inbuff);
    if (rc) {
        ESP_LOGE(TAG, "Read the Value of Body Sensor Location %s!", esp_err_to_name(rc));
        return rc;
    }

    *location = *inbuff.data;

    return ESP_OK;
}

esp_err_t esp_ble_hrp_set_ctrl(uint8_t location)
{
    if (location != BLE_HRP_CMD_RESET_ENERGY_EXPENDED || !s_ble_hrp_data.flags.energy) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_HRP_CHR_UUID16_HEART_RATE_CNTL_POINT,
        },
        .data = &location,
        .data_len = sizeof(uint8_t),
    };

    esp_err_t rc = esp_ble_conn_write(&inbuff);
    if (rc) {
        ESP_LOGE(TAG, "Write the Value of Heart Rate Control Point %s!", esp_err_to_name(rc));
        return rc;
    }

    return rc;
}

esp_err_t esp_ble_hrp_init(void)
{
    return esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, esp_ble_hrp_event, NULL);
}

esp_err_t esp_ble_hrp_deinit(void)
{
    return esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, esp_ble_hrp_event);
}
