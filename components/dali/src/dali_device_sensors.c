/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_device_sensors.c
 * @brief DALI Part 303 (Occupancy Sensor) & Part 304 (Light Sensor) helpers.
 *
 * All functions delegate to dali_103_do_instance_command() (defined in
 * dali_control_device.c) and dali_103_send_special() for DTR pre-loads.
 */

#include "esp_log.h"
#include "esp_check.h"
#include "dali_device_sensors.h"

static const char *TAG = "dali";

/* -------------------------------------------------------------------------
 * Part 303 — Occupancy Sensor
 * ------------------------------------------------------------------------- */

esp_err_t dali_303_query_occupancy(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                   uint8_t instance, bool *occupied, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && occupied != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "handle and occupied must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_instance_command(handle, addr_type, addr, instance, DALI_103_QUERY_INPUT_INST_VALUE,
                                                     false, tx_timeout_ms, &result), TAG,
                        "DALI_303_QUERY_OCCUPANCY failed");

    if (!DALI_RESULT_IS_VALID(result)) {
        return ESP_ERR_TIMEOUT;
    }

    /* Generic Part 103 input value: non-zero means active/occupied. */
    *occupied = ((uint8_t)result != 0x00U);
    ESP_LOGD(TAG, "Part 303 occupancy query: instance=%u reply=0x%02X occupied=%s",
             instance, (unsigned)result, *occupied ? "YES" : "NO");
    return ESP_OK;
}

esp_err_t dali_303_query_hold_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                    uint8_t instance, uint8_t *hold_timer, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && hold_timer != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "handle and hold_timer must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_instance_command(handle, addr_type, addr, instance,
                                                     DALI_303_QUERY_HOLD_TIMER, false,
                                                     tx_timeout_ms, &result), TAG,
                        "DALI_303_QUERY_HOLD_TIMER failed");
    if (!DALI_RESULT_IS_VALID(result)) {
        return ESP_ERR_TIMEOUT;
    }
    *hold_timer = (uint8_t)result;
    return ESP_OK;
}

esp_err_t dali_303_query_deadtime_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                        uint8_t instance, uint8_t *deadtime_timer, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && deadtime_timer != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "handle and deadtime_timer must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_instance_command(handle, addr_type, addr, instance, DALI_303_QUERY_DEADTIME_TIMER,
                                                     false, tx_timeout_ms, &result),
                        TAG, "DALI_303_QUERY_DEADTIME_TIMER failed");
    if (!DALI_RESULT_IS_VALID(result)) {
        return ESP_ERR_TIMEOUT;
    }
    *deadtime_timer = (uint8_t)result;
    return ESP_OK;
}

esp_err_t dali_303_set_hold_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                  uint8_t instance, uint8_t hold_time_s, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle must not be NULL");

    ESP_RETURN_ON_ERROR(dali_103_send_special(handle, DALI_103_SPECIAL_DTR0, hold_time_s,
                                              false, tx_timeout_ms, NULL), TAG,
                        "SET Part103 DTR0 (hold timer) failed");
    ESP_RETURN_ON_ERROR(dali_103_do_instance_command(handle, addr_type, addr, instance,
                                                     DALI_303_SET_HOLD_TIMER, true,
                                                     tx_timeout_ms, NULL), TAG,
                        "DALI_303_SET_HOLD_TIMER failed");

    ESP_LOGD(TAG, "Part 303 hold timer set: instance=%u value=%u", instance, hold_time_s);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Part 304 — Light Sensor
 * ------------------------------------------------------------------------- */

/**
 * @brief Generic 1-byte query helper for Part 304 instance commands.
 */
static esp_err_t dali_304_query_u8(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                   uint8_t instance, uint8_t command, uint8_t *value, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && value != NULL, ESP_ERR_INVALID_ARG, TAG, "handle and value must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_instance_command(handle, addr_type, addr, instance, command, false, tx_timeout_ms,
                                                     &result),
                        TAG, "Part 304 query failed");
    if (!DALI_RESULT_IS_VALID(result)) {
        return ESP_ERR_TIMEOUT;
    }
    *value = (uint8_t)result;
    return ESP_OK;
}

esp_err_t dali_304_query_hysteresis(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                    uint8_t instance, uint8_t *hysteresis, int tx_timeout_ms)
{
    return dali_304_query_u8(handle, addr_type, addr, instance, DALI_304_QUERY_HYSTERESIS, hysteresis, tx_timeout_ms);
}

esp_err_t dali_304_query_report_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                      uint8_t instance, uint8_t *report_timer, int tx_timeout_ms)
{
    return dali_304_query_u8(handle, addr_type, addr, instance, DALI_304_QUERY_REPORT_TIMER, report_timer, tx_timeout_ms);
}

esp_err_t dali_304_query_deadtime_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                        uint8_t instance, uint8_t *deadtime_timer, int tx_timeout_ms)
{
    return dali_304_query_u8(handle, addr_type, addr, instance, DALI_304_QUERY_DEADTIME_TIMER,
                             deadtime_timer, tx_timeout_ms);
}
