/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Health Thermometer Profile
 */

#include <string.h>

#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_htp.h"

static const char* TAG = "esp_htp";

ESP_EVENT_DEFINE_BASE(BLE_HTP_EVENTS);

static esp_err_t esp_ble_htp_get_temp_data(esp_ble_htp_data_t *temp_val, const uint8_t *inbuf, uint16_t inlen)
{
    if (!inbuf || inlen < 5) {
        return ESP_ERR_INVALID_ARG;
    }

    /* The Flags Field */
    memcpy(&temp_val->flags, inbuf, sizeof(temp_val->flags));
    inbuf += sizeof(temp_val->flags);

    /* Temperature Measurement Value Field */
    if (temp_val->flags.temperature_unit) {
        memcpy(&temp_val->temperature.fahrenheit, inbuf, sizeof(temp_val->temperature));
    } else {
        memcpy(&temp_val->temperature.celsius, inbuf, sizeof(temp_val->temperature));
    }
    inbuf += sizeof(temp_val->temperature);

    /* Time Stamp Field */
    if (temp_val->flags.time_stamp) {
        memcpy(&temp_val->timestamp, inbuf, sizeof(temp_val->timestamp));
        inbuf += sizeof(temp_val->timestamp);
    }

    /* Temperature Type Field */
    if (temp_val->flags.temperature_type) {
        temp_val->location = (*inbuf);
    }

    return ESP_OK;
}

static void esp_ble_htp_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DATA_RECEIVE\n");
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        esp_ble_htp_data_t   ble_htp_data;
        memset(&ble_htp_data, 0, sizeof(esp_ble_htp_data_t));
        esp_ble_htp_get_temp_data(&ble_htp_data, conn_data->data, conn_data->data_len);
        esp_event_post(BLE_HTP_EVENTS, conn_data->uuid.uuid16, &ble_htp_data, sizeof(esp_ble_htp_data_t), portMAX_DELAY);
        break;
    default:
        break;
    }
}

esp_err_t esp_ble_htp_get_temp_type(uint8_t *temp_type)
{
    if (!temp_type) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_HTP_CHR_UUID16_TEMPERATURE_TYPE,
        },
        .data = NULL,
        .data_len = 0,
    };

    esp_err_t rc = esp_ble_conn_read(&inbuff);
    if (rc) {
        ESP_LOGE(TAG, "Read the Value of Temperature Type %s!", esp_err_to_name(rc));
        return rc;
    }

    *temp_type = *inbuff.data;

    return ESP_OK;
}

esp_err_t esp_ble_htp_get_measurement_interval(uint16_t *interval_val)
{
    if (!interval_val) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_HTP_CHR_UUID16_MEASUREMENT_INTERVAL,
        },
        .data = NULL,
        .data_len = 0,
    };

    esp_err_t rc = esp_ble_conn_read(&inbuff);
    if (rc) {
        ESP_LOGE(TAG, "Read the Value of Measurement Interval %s!", esp_err_to_name(rc));
        return rc;
    }

    memcpy(interval_val, inbuff.data, MIN(inbuff.data_len, sizeof(uint16_t)));

    return ESP_OK;
}

esp_err_t esp_ble_htp_set_measurement_interval(uint16_t interval_val)
{
    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_HTP_CHR_UUID16_MEASUREMENT_INTERVAL,
        },
        .data = (uint8_t *) &interval_val,
        .data_len = sizeof(interval_val),
    };

    esp_err_t rc = esp_ble_conn_write(&inbuff);
    if (rc == 0) {
        ESP_LOGI(TAG, "Sets the measurement interval value Success!");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, inbuff.data, inbuff.data_len, ESP_LOG_INFO);
    }

    return rc;
}

esp_err_t esp_ble_htp_init(void)
{
    return esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, esp_ble_htp_event, NULL);
}

esp_err_t esp_ble_htp_deinit(void)
{
    return esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, esp_ble_htp_event);
}
