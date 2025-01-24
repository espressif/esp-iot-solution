/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Health Thermometer Service
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_hts.h"

/**
 * Structure holding data for the main characteristics
 */
typedef struct esp_ble_hts_data {
    uint8_t                 temp_type;
    uint16_t                measurement_interval;
    esp_ble_hts_temp_t  measurement_temp;
    esp_ble_hts_temp_t  intermediate_temp;
} esp_ble_hts_data_t;

static esp_ble_hts_data_t s_ble_hts_data = {
    .intermediate_temp = {
        .flags = {
            .temperature_unit = BLE_HTS_CHR_TEMPERATURE_UNITS_CELSIUS,
            .temperature_type = BLE_HTS_CHR_TEMPERATURE_FLAGS_NOT,
            .time_stamp = BLE_HTS_CHR_TEMPERATURE_FLAGS_NOT,
        },
    },
    .measurement_temp = {
        .flags = {
            .temperature_unit = BLE_HTS_CHR_TEMPERATURE_UNITS_CELSIUS,
            .temperature_type = BLE_HTS_CHR_TEMPERATURE_FLAGS_NOT,
            .time_stamp = BLE_HTS_CHR_TEMPERATURE_FLAGS_NOT,
        },
    },
    .temp_type = BLE_HTS_CHR_TEMPERATURE_TYPE_RFU
};

ESP_EVENT_DEFINE_BASE(BLE_HTS_EVENTS);

static esp_err_t ble_hts_temp_notify(bool notify, uint16_t uuid16, esp_ble_hts_temp_t *tm)
{
    uint16_t len = 0;
    uint8_t  hts_data[20] = { 0 };

    memcpy(hts_data, &tm->flags, sizeof(tm->flags));
    len += sizeof(tm->flags);

    if (tm->flags.temperature_unit) {
        memcpy(&hts_data[len], &tm->temperature.fahrenheit, sizeof(tm->temperature.fahrenheit));
    } else {
        memcpy(&hts_data[len], &tm->temperature.celsius, sizeof(tm->temperature.celsius));
    }
    len += sizeof(tm->temperature);

    if (tm->flags.time_stamp) {
        memcpy(&hts_data[len], &tm->timestamp, sizeof(tm->timestamp));
        len += sizeof(tm->timestamp);
    }

    if (tm->flags.temperature_type) {
        memcpy(&hts_data[len], &tm->location, sizeof(tm->location));
        len += sizeof(tm->location);
    }

    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = uuid16,
        },
        .data = hts_data,
        .data_len = len,
    };

    return notify ? esp_ble_conn_notify(&conn_data) : esp_ble_conn_write(&conn_data);
}

#ifdef CONFIG_BLE_HTS_TEMPERATURE_TYPE
static esp_err_t esp_hts_temperature_type_cb(const uint8_t *inbuf, uint16_t inlen,
                                             uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t len = sizeof(s_ble_hts_data.temp_type);
    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_ble_hts_data.temp_type, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_HTS_MEASUREMENT_INTERVAL
static esp_err_t esp_hts_measurement_interval_cb(const uint8_t *inbuf, uint16_t inlen,
                                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    uint8_t len = sizeof(s_ble_hts_data.measurement_interval);

    if (!outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf) {
#ifdef CONFIG_BLE_HTS_MEASUREMENT_INTERVAL_WRITE_ENABLE
        s_ble_hts_data.measurement_interval = 0;
        memcpy(&s_ble_hts_data.measurement_interval, inbuf, MIN(len, inlen));
        esp_event_post(BLE_HTS_EVENTS, BLE_HTS_CHR_UUID16_MEASUREMENT_INTERVAL, &s_ble_hts_data.measurement_interval, len, portMAX_DELAY);
#else
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
#endif
    }

    *outbuf = calloc(1, len);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    memcpy(*outbuf, &s_ble_hts_data.measurement_interval, len);
    *outlen = len;

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

esp_err_t esp_ble_hts_get_measurement_temp(esp_ble_hts_temp_t *temp_val)
{
    if (!temp_val) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(temp_val, &s_ble_hts_data.measurement_temp, sizeof(esp_ble_hts_temp_t));

    return ESP_OK;
}

esp_err_t esp_ble_hts_set_measurement_temp(esp_ble_hts_temp_t *temp_val)
{
    if (!temp_val) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_hts_data.measurement_temp, temp_val, sizeof(esp_ble_hts_temp_t));

    return ble_hts_temp_notify(false, BLE_HTS_CHR_UUID16_TEMPERATURE_MEASUREMENT, &s_ble_hts_data.measurement_temp);
}

esp_err_t esp_ble_hts_get_intermediate_temp(esp_ble_hts_temp_t *temp_val)
{
    if (!temp_val) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(temp_val, &s_ble_hts_data.intermediate_temp, sizeof(esp_ble_hts_temp_t));

    return ESP_OK;
}

esp_err_t esp_ble_hts_set_intermediate_temp(esp_ble_hts_temp_t *temp_val)
{
    if (!temp_val) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_hts_data.intermediate_temp, temp_val, sizeof(esp_ble_hts_temp_t));

    return ble_hts_temp_notify(true, BLE_HTS_CHR_UUID16_INTERMEDIATE_TEMPERATURE, &s_ble_hts_data.intermediate_temp);
}

esp_err_t esp_ble_hts_get_temp_type(uint8_t *temp_type)
{
    if (!temp_type) {
        return ESP_ERR_INVALID_ARG;
    }

    *temp_type = s_ble_hts_data.temp_type;

    return ESP_OK;
}

esp_err_t esp_ble_hts_set_temp_type(uint8_t temp_type)
{
    if (temp_type >= BLE_HTS_CHR_TEMPERATURE_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (temp_type != s_ble_hts_data.temp_type) {
        s_ble_hts_data.temp_type = temp_type;
    }

    return ESP_OK;
}

esp_err_t esp_ble_hts_get_measurement_interval(uint16_t *interval_val)
{
    if (!interval_val) {
        return ESP_ERR_INVALID_ARG;
    }

    *interval_val = s_ble_hts_data.measurement_interval;

    return ESP_OK;
}

esp_err_t esp_ble_hts_set_measurement_interval(uint16_t interval_val)
{
    if (interval_val != s_ble_hts_data.measurement_interval) {
        s_ble_hts_data.measurement_interval = interval_val;
#ifdef CONFIG_BLE_HTS_MEASUREMENT_INTERVAL_INDICATE_ENABLE
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_HTS_CHR_UUID16_MEASUREMENT_INTERVAL,
            },
            .data = (uint8_t *) &s_ble_hts_data.measurement_interval,
            .data_len = sizeof(s_ble_hts_data.measurement_interval),
        };

        return esp_ble_conn_write(&conn_data);
#endif
    }

    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {"temp_meas",     BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_INDICATE, { BLE_HTS_CHR_UUID16_TEMPERATURE_MEASUREMENT }, NULL},
#ifdef CONFIG_BLE_HTS_TEMPERATURE_TYPE
    {"temp_type",     BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ, { BLE_HTS_CHR_UUID16_TEMPERATURE_TYPE }, esp_hts_temperature_type_cb},
#endif
#ifdef CONFIG_BLE_HTS_INTERMEDIATE_TEMPERATURE
    {"intermediate_temp",            BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_NOTIFY, { BLE_HTS_CHR_UUID16_INTERMEDIATE_TEMPERATURE }, NULL},
#endif
#ifdef CONFIG_BLE_HTS_MEASUREMENT_INTERVAL
    {
        "meas_interval",     BLE_CONN_UUID_TYPE_16, BLE_CONN_GATT_CHR_READ
#ifdef CONFIG_BLE_HTS_MEASUREMENT_INTERVAL_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
#ifdef CONFIG_BLE_HTS_MEASUREMENT_INTERVAL_WRITE_ENABLE
        | BLE_CONN_GATT_CHR_WRITE
#endif
        , { BLE_HTS_CHR_UUID16_MEASUREMENT_INTERVAL }, esp_hts_measurement_interval_cb
    },
#endif
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_HTS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_hts_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
