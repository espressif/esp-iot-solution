/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Battery Service
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_bas.h"

static esp_ble_bas_data_t s_ble_bas_data;

static esp_err_t esp_ble_bas_data_handle(bool notify, uint16_t uuid16, uint8_t **outbuf, uint16_t *outlen)
{
    esp_err_t ret = ESP_OK;
    bool    indicate = false;
    uint8_t len = 0;
    uint8_t bas_data[20] = { 0 };

    switch (uuid16) {
    case BLE_BAS_CHR_UUID16_LEVEL:
        memcpy(bas_data, &s_ble_bas_data.battery_level, sizeof(s_ble_bas_data.battery_level));
        len += sizeof(s_ble_bas_data.battery_level);
        break;
    case BLE_BAS_CHR_UUID16_LEVEL_STATUS:
#ifdef CONFIG_BLE_BAS_BATTERY_LEVEL_STATUS_INDICATE_ENABLE
        indicate = true;
#endif
        memcpy(bas_data, &s_ble_bas_data.level_status.flags, sizeof(s_ble_bas_data.level_status.flags));
        len += sizeof(s_ble_bas_data.level_status.flags);

        memcpy(&bas_data[len], &s_ble_bas_data.level_status.power_state, sizeof(s_ble_bas_data.level_status.power_state));
        len += sizeof(s_ble_bas_data.level_status.power_state);

        if (s_ble_bas_data.level_status.flags.en_identifier) {
            memcpy(&bas_data[len], &s_ble_bas_data.level_status.identifier, sizeof(s_ble_bas_data.level_status.identifier));
            len += sizeof(s_ble_bas_data.level_status.identifier);
        }

        if (s_ble_bas_data.level_status.flags.en_battery_level) {
            memcpy(&bas_data[len], &s_ble_bas_data.level_status.battery_level, sizeof(s_ble_bas_data.level_status.battery_level));
            len += sizeof(s_ble_bas_data.level_status.battery_level);
        }

        if (s_ble_bas_data.level_status.flags.en_additional_status) {
            memcpy(&bas_data[len], &s_ble_bas_data.level_status.additional_status, sizeof(s_ble_bas_data.level_status.additional_status));
            len += sizeof(s_ble_bas_data.level_status.additional_status);
        }
        break;
    case BLE_BAS_CHR_UUID16_ESTIMATED_SERVICE_DATE:
#ifdef CONFIG_BLE_BAS_ESTIMATED_SERVICE_DATE_INDICATE_ENABLE
        indicate = true;
#endif
        memcpy(bas_data, &s_ble_bas_data.estimated_service_date, sizeof(s_ble_bas_data.estimated_service_date));
        len += sizeof(s_ble_bas_data.estimated_service_date);
        break;
    case BLE_BAS_CHR_UUID16_CRITICAL_STATUS:
        indicate = true;

        memcpy(bas_data, &s_ble_bas_data.critical_status, sizeof(s_ble_bas_data.critical_status));
        len += sizeof(s_ble_bas_data.critical_status);
        break;
    case BLE_BAS_CHR_UUID16_ENERGY_STATUS:
#ifdef CONFIG_BLE_BAS_BATTERY_ENERGY_STATUS_INDICATE_ENABLE
        indicate = true;
#endif
        memcpy(bas_data, &s_ble_bas_data.energy_status.flags, sizeof(s_ble_bas_data.energy_status.flags));
        len += sizeof(s_ble_bas_data.energy_status.flags);

        if (s_ble_bas_data.energy_status.flags.en_external_source_power) {
            memcpy(&bas_data[len], &s_ble_bas_data.energy_status.external_source_power, sizeof(s_ble_bas_data.energy_status.external_source_power));
            len += sizeof(s_ble_bas_data.energy_status.external_source_power);
        }

        if (s_ble_bas_data.energy_status.flags.en_voltage) {
            memcpy(&bas_data[len], &s_ble_bas_data.energy_status.voltage, sizeof(s_ble_bas_data.energy_status.voltage));
            len += sizeof(s_ble_bas_data.energy_status.voltage);
        }

        if (s_ble_bas_data.energy_status.flags.en_available_energy) {
            memcpy(&bas_data[len], &s_ble_bas_data.energy_status.available_energy, sizeof(s_ble_bas_data.energy_status.available_energy));
            len += sizeof(s_ble_bas_data.energy_status.available_energy);
        }

        if (s_ble_bas_data.energy_status.flags.en_available_battery_capacity) {
            memcpy(&bas_data[len], &s_ble_bas_data.energy_status.available_battery_capacity, sizeof(s_ble_bas_data.energy_status.available_battery_capacity));
            len += sizeof(s_ble_bas_data.energy_status.available_battery_capacity);
        }

        if (s_ble_bas_data.energy_status.flags.en_charge_rate) {
            memcpy(&bas_data[len], &s_ble_bas_data.energy_status.charge_rate, sizeof(s_ble_bas_data.energy_status.charge_rate));
            len += sizeof(s_ble_bas_data.energy_status.charge_rate);
        }

        if (s_ble_bas_data.energy_status.flags.en_available_energy_last_charge) {
            memcpy(&bas_data[len], &s_ble_bas_data.energy_status.available_energy_last_charge, sizeof(s_ble_bas_data.energy_status.available_energy_last_charge));
            len += sizeof(s_ble_bas_data.energy_status.available_energy_last_charge);
        }
        break;
    case BLE_BAS_CHR_UUID16_TIME_STATUS:
#ifdef CONFIG_BLE_BAS_BATTERY_TIME_STATUS_INDICATE_ENABLE
        indicate = true;
#endif
        memcpy(bas_data, &s_ble_bas_data.time_status.flags, sizeof(s_ble_bas_data.time_status.flags));
        len += sizeof(s_ble_bas_data.time_status.flags);

        memcpy(&bas_data[len], &s_ble_bas_data.time_status.discharged, sizeof(s_ble_bas_data.time_status.discharged));
        len += sizeof(s_ble_bas_data.time_status.discharged);

        if (s_ble_bas_data.time_status.flags.en_discharged_standby) {
            memcpy(&bas_data[len], &s_ble_bas_data.time_status.discharged_standby, sizeof(s_ble_bas_data.time_status.discharged_standby));
            len += sizeof(s_ble_bas_data.time_status.discharged_standby);
        }

        if (s_ble_bas_data.time_status.flags.en_recharged) {
            memcpy(&bas_data[len], &s_ble_bas_data.time_status.recharged, sizeof(s_ble_bas_data.time_status.recharged));
            len += sizeof(s_ble_bas_data.time_status.recharged);
        }
        break;
    case BLE_BAS_CHR_UUID16_HEALTH_STATUS:
#ifdef CONFIG_BLE_BAS_BATTERY_HEALTH_STATUS_INDICATE_ENABLE
        indicate = true;
#endif
        memcpy(bas_data, &s_ble_bas_data.health_status.flags, sizeof(s_ble_bas_data.health_status.flags));
        len += sizeof(s_ble_bas_data.health_status.flags);

        if (s_ble_bas_data.health_status.flags.en_battery_health_summary) {
            memcpy(&bas_data[len], &s_ble_bas_data.health_status.battery_health_summary, sizeof(s_ble_bas_data.health_status.battery_health_summary));
            len += sizeof(s_ble_bas_data.health_status.battery_health_summary);
        }

        if (s_ble_bas_data.health_status.flags.en_cycle_count) {
            memcpy(&bas_data[len], &s_ble_bas_data.health_status.cycle_count, sizeof(s_ble_bas_data.health_status.cycle_count));
            len += sizeof(s_ble_bas_data.health_status.cycle_count);
        }

        if (s_ble_bas_data.health_status.flags.en_current_temperature) {
            memcpy(&bas_data[len], &s_ble_bas_data.health_status.current_temperature, sizeof(s_ble_bas_data.health_status.current_temperature));
            len += sizeof(s_ble_bas_data.health_status.current_temperature);
        }

        if (s_ble_bas_data.health_status.flags.en_deep_discharge_count) {
            memcpy(&bas_data[len], &s_ble_bas_data.health_status.deep_discharge_count, sizeof(s_ble_bas_data.health_status.deep_discharge_count));
            len += sizeof(s_ble_bas_data.health_status.deep_discharge_count);
        }
        break;
    case BLE_BAS_CHR_UUID16_HEALTH_INFO:
        indicate = true;

        memcpy(bas_data, &s_ble_bas_data.health_info.flags, sizeof(s_ble_bas_data.health_info.flags));
        len += sizeof(s_ble_bas_data.health_info.flags);

        if (s_ble_bas_data.health_info.flags.en_cycle_count_designed_lifetime) {
            memcpy(&bas_data[len], &s_ble_bas_data.health_info.cycle_count_designed_lifetime, sizeof(s_ble_bas_data.health_info.cycle_count_designed_lifetime));
            len += sizeof(s_ble_bas_data.health_info.cycle_count_designed_lifetime);
        }

        if (s_ble_bas_data.health_info.flags.min_max_designed_operating_temperature) {
            memcpy(&bas_data[len], &s_ble_bas_data.health_info.min_designed_operating_temperature, sizeof(s_ble_bas_data.health_info.min_designed_operating_temperature));
            len += sizeof(s_ble_bas_data.health_info.min_designed_operating_temperature);

            memcpy(&bas_data[len], &s_ble_bas_data.health_info.max_designed_operating_temperature, sizeof(s_ble_bas_data.health_info.max_designed_operating_temperature));
            len += sizeof(s_ble_bas_data.health_info.max_designed_operating_temperature);
        }
        break;
    case BLE_BAS_CHR_UUID16_BATTERY_INFO:
        indicate = true;

        memcpy(bas_data, &s_ble_bas_data.battery_info.flags, sizeof(s_ble_bas_data.battery_info.flags));
        len += sizeof(s_ble_bas_data.battery_info.flags);

        memcpy(&bas_data[len], &s_ble_bas_data.battery_info.features, sizeof(s_ble_bas_data.battery_info.features));
        len += sizeof(s_ble_bas_data.battery_info.features);

        if (s_ble_bas_data.battery_info.flags.en_manufacture_date) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.manufacture_date, sizeof(s_ble_bas_data.battery_info.manufacture_date));
            len += sizeof(s_ble_bas_data.battery_info.manufacture_date);
        }

        if (s_ble_bas_data.battery_info.flags.en_expiration_date) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.expiration_date, sizeof(s_ble_bas_data.battery_info.expiration_date));
            len += sizeof(s_ble_bas_data.battery_info.expiration_date);
        }

        if (s_ble_bas_data.battery_info.flags.en_designed_capacity) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.designed_capacity, sizeof(s_ble_bas_data.battery_info.designed_capacity));
            len += sizeof(s_ble_bas_data.battery_info.designed_capacity);
        }

        if (s_ble_bas_data.battery_info.flags.en_low_energy) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.low_energy, sizeof(s_ble_bas_data.battery_info.low_energy));
            len += sizeof(s_ble_bas_data.battery_info.low_energy);
        }

        if (s_ble_bas_data.battery_info.flags.en_critical_energy) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.critical_energy, sizeof(s_ble_bas_data.battery_info.critical_energy));
            len += sizeof(s_ble_bas_data.battery_info.critical_energy);
        }

        if (s_ble_bas_data.battery_info.flags.en_chemistry) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.chemistry, sizeof(s_ble_bas_data.battery_info.chemistry));
            len += sizeof(s_ble_bas_data.battery_info.chemistry);
        }

        if (s_ble_bas_data.battery_info.flags.en_nominalvoltage) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.nominalvoltage, sizeof(s_ble_bas_data.battery_info.nominalvoltage));
            len += sizeof(s_ble_bas_data.battery_info.nominalvoltage);
        }

        if (s_ble_bas_data.battery_info.flags.en_aggregation_group) {
            memcpy(&bas_data[len], &s_ble_bas_data.battery_info.aggregation_group, sizeof(s_ble_bas_data.battery_info.aggregation_group));
            len += sizeof(s_ble_bas_data.battery_info.aggregation_group);
        }
        break;
    default:
        break;
    }

    if (notify) {
        esp_ble_conn_data_t conn_data = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = uuid16,
            },
            .data = bas_data,
            .data_len = len,
        };

        return indicate ? esp_ble_conn_write(&conn_data) : esp_ble_conn_notify(&conn_data);
    } else {
        *outbuf = calloc(1, len);
        if (!(*outbuf)) {
            return ESP_ERR_NO_MEM;
        }

        memcpy(*outbuf, bas_data, len);
        *outlen = len;
    }

    return ret;
}

static esp_err_t esp_bas_level_cb(const uint8_t *inbuf, uint16_t inlen,
                                  uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_LEVEL, outbuf, outlen);
}

#ifdef CONFIG_BLE_BAS_BATTERY_LEVEL_STATUS
static esp_err_t esp_bas_level_status_cb(const uint8_t *inbuf, uint16_t inlen,
                                         uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_LEVEL_STATUS, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_ESTIMATED_SERVICE_DATE
static esp_err_t esp_bas_estimated_date_cb(const uint8_t *inbuf, uint16_t inlen,
                                           uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_ESTIMATED_SERVICE_DATE, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_CRITICAL_STATUS
static esp_err_t esp_bas_critical_status_cb(const uint8_t *inbuf, uint16_t inlen,
                                            uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_CRITICAL_STATUS, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_ENERGY_STATUS
static esp_err_t esp_bas_energy_status_cb(const uint8_t *inbuf, uint16_t inlen,
                                          uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_ENERGY_STATUS, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_TIME_STATUS
static esp_err_t esp_bas_time_status_cb(const uint8_t *inbuf, uint16_t inlen,
                                        uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_TIME_STATUS, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_HEALTH_STATUS
static esp_err_t esp_bas_health_status_cb(const uint8_t *inbuf, uint16_t inlen,
                                          uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_HEALTH_STATUS, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_HEALTH_INFORMATION
static esp_err_t esp_bas_health_info_cb(const uint8_t *inbuf, uint16_t inlen,
                                        uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_HEALTH_INFO, outbuf, outlen);
}
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_INFORMATION
static esp_err_t esp_bas_battery_info_cb(const uint8_t *inbuf, uint16_t inlen,
                                         uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    *att_status = ESP_IOT_ATT_SUCCESS;

    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    return esp_ble_bas_data_handle(false, BLE_BAS_CHR_UUID16_BATTERY_INFO, outbuf, outlen);
}
#endif

esp_err_t esp_ble_bas_get_battery_level(uint8_t *level)
{
    if (!level) {
        return ESP_ERR_INVALID_ARG;
    }

    *level = s_ble_bas_data.battery_level;

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_battery_level(uint8_t *level)
{
    if (!level) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((*level) > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_ble_bas_data.battery_level != (*level)) {
        s_ble_bas_data.battery_level = (*level);
#ifdef CONFIG_BLE_BAS_BATTERY_LEVEL_NOTIFY_ENABLE
        return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_LEVEL, NULL, 0);
#endif
    }

    return ESP_OK;
}

esp_err_t esp_ble_bas_get_level_status(esp_ble_bas_level_status_t *level_status)
{
    if (!level_status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(level_status, &s_ble_bas_data.level_status, sizeof(esp_ble_bas_level_status_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_level_status(esp_ble_bas_level_status_t *level_status)
{
    if (!level_status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.level_status, level_status, sizeof(esp_ble_bas_level_status_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_LEVEL_STATUS, NULL, 0);
}

esp_err_t esp_ble_bas_get_estimated_date(uint32_t *estimated_date)
{
    if (!estimated_date) {
        return ESP_ERR_INVALID_ARG;
    }

    *estimated_date = s_ble_bas_data.estimated_service_date.u24;

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_estimated_date(uint32_t *estimated_date)
{
    if (!estimated_date) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_ble_bas_data.estimated_service_date.u24 != (*estimated_date)) {
        s_ble_bas_data.estimated_service_date.u24 = (*estimated_date);
    }

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_ESTIMATED_SERVICE_DATE, NULL, 0);
}

esp_err_t esp_ble_bas_get_critical_status(esp_ble_bas_critical_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(status, &s_ble_bas_data.critical_status, sizeof(esp_ble_bas_critical_status_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_critical_status(esp_ble_bas_critical_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.critical_status, status, sizeof(esp_ble_bas_critical_status_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_CRITICAL_STATUS, NULL, 0);
}

esp_err_t esp_ble_bas_get_energy_status(esp_ble_bas_energy_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(status, &s_ble_bas_data.energy_status, sizeof(esp_ble_bas_energy_status_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_energy_status(esp_ble_bas_energy_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.energy_status, status, sizeof(esp_ble_bas_energy_status_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_ENERGY_STATUS, NULL, 0);
}

esp_err_t esp_ble_bas_get_time_status(esp_ble_bas_time_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(status, &s_ble_bas_data.time_status, sizeof(esp_ble_bas_time_status_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_time_status(esp_ble_bas_time_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.time_status, status, sizeof(esp_ble_bas_time_status_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_TIME_STATUS, NULL, 0);
}

esp_err_t esp_ble_bas_get_health_status(esp_ble_bas_health_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(status, &s_ble_bas_data.health_status, sizeof(esp_ble_bas_health_status_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_health_status(esp_ble_bas_health_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.health_status, status, sizeof(esp_ble_bas_health_status_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_HEALTH_STATUS, NULL, 0);
}

esp_err_t esp_ble_bas_get_health_info(esp_ble_bas_health_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(info, &s_ble_bas_data.health_info, sizeof(esp_ble_bas_health_info_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_health_info(esp_ble_bas_health_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.health_info, info, sizeof(esp_ble_bas_health_info_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_HEALTH_INFO, NULL, 0);
}

esp_err_t esp_ble_bas_get_battery_info(esp_ble_bas_battery_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(info, &s_ble_bas_data.battery_info, sizeof(esp_ble_bas_battery_info_t));

    return ESP_OK;
}

esp_err_t esp_ble_bas_set_battery_info(esp_ble_bas_battery_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_bas_data.battery_info, info, sizeof(esp_ble_bas_battery_info_t));

    return esp_ble_bas_data_handle(true, BLE_BAS_CHR_UUID16_BATTERY_INFO, NULL, 0);
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "level",     BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ
#ifdef CONFIG_BLE_BAS_BATTERY_LEVEL_NOTIFY_ENABLE
        | BLE_CONN_GATT_CHR_NOTIFY
#endif
        , { BLE_BAS_CHR_UUID16_LEVEL },   esp_bas_level_cb
    },
#ifdef CONFIG_BLE_BAS_BATTERY_LEVEL_STATUS
    {
        "level_status",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY
#ifdef CONFIG_BLE_BAS_BATTERY_LEVEL_STATUS_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
        , { BLE_BAS_CHR_UUID16_LEVEL_STATUS},       esp_bas_level_status_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_ESTIMATED_SERVICE_DATE
    {
        "estimated_date",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY
#ifdef CONFIG_BLE_BAS_ESTIMATED_SERVICE_DATE_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
        , { BLE_BAS_CHR_UUID16_ESTIMATED_SERVICE_DATE},       esp_bas_estimated_date_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_CRITICAL_STATUS
    {
        "critical_status",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_INDICATE
        , { BLE_BAS_CHR_UUID16_CRITICAL_STATUS},       esp_bas_critical_status_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_ENERGY_STATUS
    {
        "energy_status",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY
#ifdef CONFIG_BLE_BAS_BATTERY_ENERGY_STATUS_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
        , { BLE_BAS_CHR_UUID16_ENERGY_STATUS},       esp_bas_energy_status_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_TIME_STATUS
    {
        "time_status",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY
#ifdef CONFIG_BLE_BAS_BATTERY_TIME_STATUS_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
        , { BLE_BAS_CHR_UUID16_TIME_STATUS},       esp_bas_time_status_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_HEALTH_STATUS
    {
        "health_status",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY
#ifdef CONFIG_BLE_BAS_BATTERY_HEALTH_STATUS_INDICATE_ENABLE
        | BLE_CONN_GATT_CHR_INDICATE
#endif
        , { BLE_BAS_CHR_UUID16_HEALTH_STATUS},       esp_bas_health_status_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_HEALTH_INFORMATION
    {
        "health_info",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_INDICATE
        , { BLE_BAS_CHR_UUID16_HEALTH_INFO},       esp_bas_health_info_cb
    },
#endif

#ifdef CONFIG_BLE_BAS_BATTERY_INFORMATION
    {
        "battery_info",    BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_INDICATE
        , { BLE_BAS_CHR_UUID16_BATTERY_INFO},       esp_bas_battery_info_cb
    },
#endif
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_BAS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_bas_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
