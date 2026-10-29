/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_color_control_dt8.c
 * @brief DALI Part 209 — Color Control (DT8) implementation.
 *
 * Implements dali_enable_device_type() and dali_master_set_color() for
 * CCT, RGB, and RGBWAF color modes.
 */

#include "esp_log.h"
#include "esp_check.h"
#include "dali_color_control_dt8.h"

static const char *TAG = "dali";

/* -------------------------------------------------------------------------
 * Private helper — DTR pre-load
 * ------------------------------------------------------------------------- */

/**
 * @brief Send a DTR-load special command (broadcast).
 */
static esp_err_t dali_set_dtr(dali_master_handle_t handle, uint8_t dtr_addr_byte, uint8_t value, int tx_timeout_ms)
{
    dali_master_transaction_config_t cfg = {
        .addr_type     = DALI_ADDR_SPECIAL,
        .addr          = dtr_addr_byte,
        .is_cmd        = false,
        .command       = value,
        .send_twice    = false,
        .tx_timeout_ms = tx_timeout_ms,
    };
    return dali_master_do_transaction(handle, &cfg, NULL);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

esp_err_t dali_enable_device_type(dali_master_handle_t handle, uint8_t device_type, int tx_timeout_ms)
{
    dali_master_transaction_config_t cfg = {
        .addr_type     = DALI_ADDR_SPECIAL,
        .addr          = DALI_SPECIAL_ENABLE_DEVICE_TYPE,
        .is_cmd        = false,
        .command       = device_type,
        .send_twice    = false,
        .tx_timeout_ms = tx_timeout_ms,
    };
    return dali_master_do_transaction(handle, &cfg, NULL);
}

esp_err_t dali_master_set_color(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                dali_color_mode_t mode, dali_color_val_t val, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle must not be NULL");

    esp_err_t err;
    uint8_t color_cmd;

    if (mode == DALI_COLOR_CCT) {
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG,
                                         (uint8_t)(val.cct.mirek & 0xFFU), tx_timeout_ms),
                            TAG, "SET DTR0 failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG1,
                                         (uint8_t)(val.cct.mirek >> 8), tx_timeout_ms),
                            TAG, "SET DTR1 failed");
        color_cmd = DALI_209_SET_COLOR_TEMPERATURE;
        ESP_LOGD(TAG, "Part 209 CCT: addr_type=%d addr=%u mirek=%u", addr_type, addr, val.cct.mirek);

    } else if (mode == DALI_COLOR_RGB) {
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG, val.rgb.r, tx_timeout_ms),
                            TAG, "SET DTR0 (R) failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG1, val.rgb.g, tx_timeout_ms),
                            TAG, "SET DTR1 (G) failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG2, val.rgb.b, tx_timeout_ms),
                            TAG, "SET DTR2 (B) failed");
        color_cmd = DALI_209_SET_TEMPORARY_RGB_DIMLEVEL;
        ESP_LOGD(TAG, "Part 209 RGB: addr_type=%d addr=%u r=%u g=%u b=%u",
                 addr_type, addr, val.rgb.r, val.rgb.g, val.rgb.b);

    } else if (mode == DALI_COLOR_RGBWAF) {
        /* Step 1: Load R/G/B; send SET_TEMPORARY_RGB_DIMLEVEL. */
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG,  val.rgbwaf.r, tx_timeout_ms),
                            TAG, "SET DTR0 (R) failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG1, val.rgbwaf.g, tx_timeout_ms),
                            TAG, "SET DTR1 (G) failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG2, val.rgbwaf.b, tx_timeout_ms),
                            TAG, "SET DTR2 (B) failed");

        dali_master_transaction_config_t rgb_cfg = {
            .addr_type     = addr_type,
            .addr          = addr,
            .is_cmd        = true,
            .command       = DALI_209_SET_TEMPORARY_RGB_DIMLEVEL,
            .send_twice    = false,
            .tx_timeout_ms = tx_timeout_ms,
        };
        ESP_RETURN_ON_ERROR(dali_enable_device_type(handle, 8, tx_timeout_ms),
                            TAG, "ENABLE_DEVICE_TYPE 8 before RGB failed");
        ESP_RETURN_ON_ERROR(dali_master_do_transaction(handle, &rgb_cfg, NULL),
                            TAG, "SET_TEMPORARY_RGB_DIMLEVEL failed");

        /* Step 2: Load W/A/F; fall through to send SET_TEMPORARY_WAF_DIMLEVEL below. */
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG,  val.rgbwaf.w, tx_timeout_ms),
                            TAG, "SET DTR0 (W) failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG1, val.rgbwaf.a, tx_timeout_ms),
                            TAG, "SET DTR1 (A) failed");
        ESP_RETURN_ON_ERROR(dali_set_dtr(handle, DALI_SPECIAL_DATA_TRANSFER_REG2, val.rgbwaf.f, tx_timeout_ms),
                            TAG, "SET DTR2 (F) failed");

        color_cmd = DALI_209_SET_TEMPORARY_WAF_DIMLEVEL;
        ESP_LOGD(TAG, "Part 209 RGBWAF: addr_type=%d addr=%u r=%u g=%u b=%u w=%u a=%u f=%u",
                 addr_type, addr,
                 val.rgbwaf.r, val.rgbwaf.g, val.rgbwaf.b,
                 val.rgbwaf.w, val.rgbwaf.a, val.rgbwaf.f);

    } else {
        ESP_LOGE(TAG, "Unknown color mode: %d", (int)mode);
        return ESP_ERR_INVALID_ARG;
    }

    /* Send SET_COLOR_TEMPERATURE / SET_RGB / SET_WAF to the addressed gear. */
    dali_master_transaction_config_t color_cfg = {
        .addr_type     = addr_type,
        .addr          = addr,
        .is_cmd        = true,
        .command       = color_cmd,
        .send_twice    = false,
        .tx_timeout_ms = tx_timeout_ms,
    };
    err = dali_enable_device_type(handle, 8, tx_timeout_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ENABLE_DEVICE_TYPE 8 before SET_COLOR failed: %s", esp_err_to_name(err));
        return err;
    }
    err = dali_master_do_transaction(handle, &color_cfg, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SET_COLOR command failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Activate: commit the new color. */
    dali_master_transaction_config_t act_cfg = {
        .addr_type     = addr_type,
        .addr          = addr,
        .is_cmd        = true,
        .command       = DALI_209_ACTIVATE,
        .send_twice    = false,
        .tx_timeout_ms = tx_timeout_ms,
    };
    err = dali_enable_device_type(handle, 8, tx_timeout_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ENABLE_DEVICE_TYPE 8 before ACTIVATE failed: %s", esp_err_to_name(err));
        return err;
    }
    err = dali_master_do_transaction(handle, &act_cfg, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ACTIVATE command failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
