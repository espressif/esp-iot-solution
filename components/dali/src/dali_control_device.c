/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_control_device.c
 * @brief DALI Part 103 — Input Device commissioning and general helpers.
 *
 * Implements:
 *   - dali_103_send_special()        : 3-byte special-command frame
 *   - dali_103_do_device_command()   : 3-byte device-level command frame
 *   - dali_103_do_instance_command() : 3-byte instance-level command frame
 *   - dali_103_commission()          : binary-search address assignment
 *   - dali_103_query_device_status() / dali_103_reset_device()
 *   - dali_103_query_instance_type() / dali_103_query_number_of_instances()
 *
 * The dali_103_send_special / dali_103_do_device_command helpers are also
 * called by dali_control_gear.c (to silence Part 103 devices during Part 102
 * commissioning) and by dali_device_sensors.c.
 */

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dali_control_device.h"
#include "dali_command.h"

static const char *TAG = "dali";

/** Minimum delay after RANDOMISE before issuing COMPARE/SEARCH_ADDR.
 *  IEC 62386-103 requires all input devices to complete 24-bit random address
 *  generation within this window. */
#define DALI_RANDOMISE_SETTLE_MS  100U

/* -------------------------------------------------------------------------
 * Internal helpers — also used by dali_control_gear.c and dali_device_sensors.c
 * ------------------------------------------------------------------------- */

esp_err_t dali_103_send_special(dali_master_handle_t handle, uint8_t special_cmd, uint8_t data, bool send_twice,
                                int tx_timeout_ms, int *result)
{
    const uint8_t tx_buf[3] = {DALI_103_SPECIAL_ADDR, special_cmd, data};
    return dali_master_do_raw_transaction(handle, tx_buf, sizeof(tx_buf), send_twice, tx_timeout_ms, result);
}

esp_err_t dali_103_do_device_command(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                     uint8_t command, bool send_twice, int tx_timeout_ms, int *result)
{
    uint8_t dest;
    switch (addr_type) {
    case DALI_ADDR_SHORT:
        ESP_RETURN_ON_FALSE(addr <= 63, ESP_ERR_INVALID_ARG, TAG, "input device short address out of range: %d", addr);
        dest = (uint8_t)((addr << 1) | 0x01U);
        break;
    case DALI_ADDR_BROADCAST:
        dest = 0xFFU;
        break;
    default:
        ESP_LOGE(TAG, "Part 103 device commands currently support short/broadcast addressing only");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t tx_buf[3] = {dest, 0xFEU, command};
    return dali_master_do_raw_transaction(handle, tx_buf, sizeof(tx_buf), send_twice, tx_timeout_ms, result);
}

esp_err_t dali_103_do_instance_command(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                       uint8_t instance, uint8_t command, bool send_twice,
                                       int tx_timeout_ms, int *result)
{
    ESP_RETURN_ON_FALSE(instance <= 31, ESP_ERR_INVALID_ARG, TAG, "instance must be 0-31");

    uint8_t dest;
    switch (addr_type) {
    case DALI_ADDR_SHORT:
        ESP_RETURN_ON_FALSE(addr <= 63, ESP_ERR_INVALID_ARG, TAG, "input device short address out of range: %d", addr);
        dest = (uint8_t)((addr << 1) | 0x01U);
        break;
    case DALI_ADDR_BROADCAST:
        dest = 0xFFU;
        break;
    default:
        ESP_LOGE(TAG, "Part 103 instance commands currently support short/broadcast addressing only");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t tx_buf[3] = {dest, instance, command};
    return dali_master_do_raw_transaction(handle, tx_buf, sizeof(tx_buf), send_twice, tx_timeout_ms, result);
}

/* -------------------------------------------------------------------------
 * Private helpers — commissioning internals
 * ------------------------------------------------------------------------- */

/**
 * @brief Set the 24-bit Part 103 search address (H/M/L bytes).
 */
static esp_err_t dali_103_set_search_addr_internal(dali_master_handle_t handle, uint32_t addr, int tx_timeout_ms)
{
    esp_err_t err;
    err = dali_103_send_special(handle, DALI_103_SPECIAL_SEARCH_ADDR_H, (uint8_t)((addr >> 16) & 0xFFU),
                                false, tx_timeout_ms, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = dali_103_send_special(handle, DALI_103_SPECIAL_SEARCH_ADDR_M, (uint8_t)((addr >> 8) & 0xFFU),
                                false, tx_timeout_ms, NULL);
    if (err != ESP_OK) {
        return err;
    }
    return dali_103_send_special(handle, DALI_103_SPECIAL_SEARCH_ADDR_L, (uint8_t)(addr & 0xFFU),
                                 false, tx_timeout_ms, NULL);
}

/**
 * @brief Issue Part 103 COMPARE and return whether any device replied YES.
 */
static esp_err_t dali_103_compare_internal(dali_master_handle_t handle, int tx_timeout_ms, bool *yes_out)
{
    int result = DALI_RESULT_NO_REPLY;
    esp_err_t err = dali_103_send_special(handle, DALI_103_SPECIAL_COMPARE, 0x00U, false, tx_timeout_ms, &result);
    if (err != ESP_OK) {
        return err;
    }
    *yes_out = (result == 0xFF);
    return ESP_OK;
}

/**
 * @brief Send a Part 102 special command — used inside dali_103_commission()
 *        to expel Part 102 gear that may misinterpret the 0xC1 first byte.
 */
static esp_err_t dali_102_send_special_local(dali_master_handle_t handle, uint8_t special_byte, uint8_t data,
                                             bool send_twice, int tx_timeout_ms, int *result)
{
    dali_master_transaction_config_t cfg = {
        .addr_type     = DALI_ADDR_SPECIAL,
        .addr          = special_byte,
        .is_cmd        = false,
        .command       = data,
        .send_twice    = send_twice,
        .tx_timeout_ms = tx_timeout_ms,
    };
    return dali_master_do_transaction(handle, &cfg, result);
}

/* -------------------------------------------------------------------------
 * Public API — commissioning
 * ------------------------------------------------------------------------- */

esp_err_t dali_103_commission(dali_master_handle_t handle, dali_commission_mode_t mode, uint8_t start_addr,
                              uint8_t max_devices, uint8_t *count, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle must not be NULL");
    ESP_RETURN_ON_FALSE(max_devices > 0 && max_devices <= 64, ESP_ERR_INVALID_ARG, TAG, "max_devices must be 1-64");
    ESP_RETURN_ON_FALSE((uint32_t)start_addr + max_devices <= 64, ESP_ERR_INVALID_ARG, TAG,
                        "address range exceeds 0-63");

    uint8_t found = 0;
    esp_err_t ret = ESP_OK;

    /* Step 0: Send Part 102 TERMINATE first, then wait for gear to settle.
     * Part 102 gear in commissioning mode may misinterpret the 0xC1 first byte
     * of Part 103 special frames (same as ENABLE_DEVICE_TYPE).  TERMINATE exits
     * the 102 commissioning session; the settle delay ensures all gear have
     * returned to normal operation before we start the Part 103 sequence. */
    ESP_RETURN_ON_ERROR(dali_102_send_special_local(handle, DALI_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL),
                        TAG, "[103 commission] Part102 TERMINATE failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Step 1: TERMINATE — abort any ongoing Part 103 commissioning session. */
    ESP_RETURN_ON_ERROR(dali_103_send_special(handle, DALI_103_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL),
                        TAG, "[103 commission] TERMINATE failed");

    /* Step 2: INITIALISE (send-twice).
     * Part 103 INITIALISE parameter is a device address byte:
     * 0xFF selects all input devices; 0x00 selects devices without short address.
     *
     * From this point on, all Part 103 devices are in INITIALISE mode.  Any
     * early-exit path MUST send TERMINATE so devices do not remain stuck in
     * commissioning state (DALI standard allows up to 15 minutes before
     * auto-exit, during which COMPARE/RANDOMISE disturbs normal bus operation).
     * Use goto cleanup to guarantee TERMINATE is always sent on error. */
    uint8_t init_byte = (mode == DALI_COMMISSION_UNADDRESSED) ? 0x00U : 0xFFU;
    ret = dali_103_send_special(handle, DALI_103_SPECIAL_INITIALIZE, init_byte, true, tx_timeout_ms, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[103 commission] INITIALISE failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* Step 2b: If any Part 102 gear misinterpreted the 0xC1 frame, terminate its
     * commissioning state before Part 103 RANDOMISE/COMPARE starts. */
    ret = dali_102_send_special_local(handle, DALI_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[103 commission] Part102 TERMINATE(post-init) failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Step 3: RANDOMISE (send-twice) — each device generates a new 24-bit random address.
     * IEC 62386-103 requires at least DALI_RANDOMISE_SETTLE_MS after RANDOMISE
     * for all devices to complete generation. */
    ret = dali_103_send_special(handle, DALI_103_SPECIAL_RANDOMIZE, 0x00U, true, tx_timeout_ms, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[103 commission] RANDOMISE failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    vTaskDelay(pdMS_TO_TICKS(DALI_RANDOMISE_SETTLE_MS));

    /* Step 4: Binary-search loop */
    {
        uint8_t next_addr = start_addr;
        ESP_LOGI(TAG, "[103 commission] Step4: binary search start (start_addr=%d, max_devices=%d)", start_addr,
                 max_devices);

        while (next_addr < (uint8_t)(start_addr + max_devices)) {
            ret = dali_103_set_search_addr_internal(handle, 0xFFFFFFUL, tx_timeout_ms);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[103 commission] SET_SEARCH_ADDR(max) failed: %s", esp_err_to_name(ret));
                goto cleanup;
            }
            bool yes = false;
            ret = dali_103_compare_internal(handle, tx_timeout_ms, &yes);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[103 commission] COMPARE(max) failed: %s", esp_err_to_name(ret));
                goto cleanup;
            }

            ESP_LOGI(TAG, "[103 commission] COMPARE(0xFFFFFF) -> %s (slot %d)",
                     yes ? "YES (device found)" : "NO (no more devices)", next_addr);

            if (!yes) {
                break;
            }

            uint32_t low  = 0x000000UL;
            uint32_t high = 0xFFFFFFUL;

            while (low < high) {
                uint32_t mid = low + (high - low) / 2U;

                ret = dali_103_set_search_addr_internal(handle, mid, tx_timeout_ms);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "[103 commission] SET_SEARCH_ADDR(bisect) failed: %s", esp_err_to_name(ret));
                    goto cleanup;
                }
                ret = dali_103_compare_internal(handle, tx_timeout_ms, &yes);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "[103 commission] COMPARE(bisect) failed: %s", esp_err_to_name(ret));
                    goto cleanup;
                }

                if (yes) {
                    high = mid;
                } else {
                    low  = mid + 1U;
                }
            }

            ret = dali_103_set_search_addr_internal(handle, low, tx_timeout_ms);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[103 commission] SET_SEARCH_ADDR(final) failed: %s", esp_err_to_name(ret));
                goto cleanup;
            }

            /* Part 103 PROGRAM_SHORT_ADDR parameter is the raw 6-bit control-device
             * short address (0..63), not the Part 102 encoded address byte.
             * Sending (addr << 1) | 1 would program address 2*addr+1. */
            uint8_t encoded = next_addr;
            ret = dali_103_send_special(handle, DALI_103_SPECIAL_PROGRAM_SHORT_ADDR, encoded, true, tx_timeout_ms,
                                        NULL);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[103 commission] PROGRAM_SHORT_ADDR failed: %s", esp_err_to_name(ret));
                goto cleanup;
            }

            ESP_LOGI(TAG, "[103 commission] Assigned short address %d (random=0x%06"PRIX32")", next_addr, low);

            ret = dali_103_send_special(handle, DALI_103_SPECIAL_WITHDRAW, 0x00U, false, tx_timeout_ms, NULL);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[103 commission] WITHDRAW failed: %s", esp_err_to_name(ret));
                goto cleanup;
            }

            next_addr++;
            found++;
        }
    }

cleanup:
    /* Step 5: TERMINATE — always sent to exit INITIALISE mode, regardless of
     * whether the binary-search loop completed successfully or hit an error.
     * Log but ignore TERMINATE's own return value so the original error is
     * preserved in ret. */
    {
        esp_err_t term_err = dali_103_send_special(handle, DALI_103_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms,
                                                   NULL);
        if (term_err != ESP_OK) {
            ESP_LOGW(TAG, "[103 commission] TERMINATE(final) failed: %s", esp_err_to_name(term_err));
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[103 commission] Aborted after %d device(s) addressed", found);
        return ret;
    }

    /* Allow devices to settle after leaving commissioning mode.
     * Some Part 103 devices (e.g. DLS-203-P) need several hundred milliseconds
     * after TERMINATE before responding to normal device commands. */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Keep newly-addressed devices in quiescent mode so they do not start
     * pushing event frames onto the shared bus immediately.  The caller
     * decides when to re-enable event reporting via STOP_QUIESCENT_MODE. */
    ESP_RETURN_ON_ERROR(dali_103_do_device_command(handle, DALI_ADDR_BROADCAST, 0, DALI_103_START_QUIESCENT_MODE,
                                                   true, tx_timeout_ms, NULL), TAG,
                        "[103 commission] START_QUIESCENT_MODE failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "[103 commission] Complete: %d input device(s) addressed (start_addr=%d)", found, start_addr);

    if (count != NULL) {
        *count = found;
    }
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Public API — device-level queries
 * ------------------------------------------------------------------------- */

esp_err_t dali_103_query_device_status(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                       uint8_t *status, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && status != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "handle and status must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_device_command(handle, addr_type, addr, DALI_103_QUERY_DEVICE_STATUS,
                                                   false, tx_timeout_ms, &result),
                        TAG, "QUERY_DEVICE_STATUS failed");
    if (!DALI_RESULT_IS_VALID(result)) {
        return ESP_ERR_TIMEOUT;
    }

    *status = (uint8_t)result;
    return ESP_OK;
}

esp_err_t dali_103_reset_device(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle must not be NULL");
    return dali_103_do_device_command(handle, addr_type, addr, DALI_103_RESET, true, tx_timeout_ms, NULL);
}

esp_err_t dali_103_query_instance_type(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                       uint8_t instance, uint8_t *type, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && type != NULL, ESP_ERR_INVALID_ARG, TAG, "handle and type must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_instance_command(handle, addr_type, addr, instance, DALI_103_QUERY_INSTANCE_TYPE,
                                                     false, tx_timeout_ms, &result),
                        TAG, "QUERY_INSTANCE_TYPE failed");
    if (!DALI_RESULT_IS_VALID(result)) {
        return ESP_ERR_TIMEOUT;
    }
    *type = (uint8_t)result;
    return ESP_OK;
}

esp_err_t dali_103_query_number_of_instances(dali_master_handle_t handle, dali_addr_type_t addr_type,
                                             uint8_t addr, uint8_t *num_instances, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL && num_instances != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "handle and num_instances must not be NULL");

    int result = DALI_RESULT_NO_REPLY;
    ESP_RETURN_ON_ERROR(dali_103_do_device_command(handle, addr_type, addr, DALI_103_QUERY_NUMBER_OF_INSTANCES,
                                                   false, tx_timeout_ms, &result), TAG,
                        "QUERY_NUMBER_OF_INSTANCES failed");

    if (!DALI_RESULT_IS_VALID(result)) {
        ESP_LOGW(TAG, "QUERY_NUMBER_OF_INSTANCES: no reply from device");
        return ESP_ERR_TIMEOUT;
    }
    *num_instances = (uint8_t)result;
    return ESP_OK;
}
