/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_control_gear.c
 * @brief DALI Part 102 — Control Gear commissioning (binary-search address assignment).
 *
 * Implements dali_commission() using the Part 102 special-command set.
 * When CONFIG_DALI_PART103_ENABLED is set, also calls into dali_control_device.c
 * helpers (dali_103_send_special / dali_103_do_device_command) to silence
 * Part 103 input devices during the COMPARE windows.
 */

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "dali_control_gear.h"
#if CONFIG_DALI_PART103_ENABLED
#include "dali_control_device.h"   /* dali_103_send_special, dali_103_do_device_command */
#endif

static const char *TAG = "dali";

/** Minimum delay after RANDOMISE before issuing COMPARE/SEARCH_ADDR.
 *  IEC 62386-102 §11.3 requires all gear to complete 24-bit random address
 *  generation within this window. */
#define DALI_RANDOMISE_SETTLE_MS  100U

/* -------------------------------------------------------------------------
 * Private helpers — Part 102 special-command wrappers
 * ------------------------------------------------------------------------- */

/**
 * @brief Send a Part 102 special command with a data byte.
 */
static esp_err_t dali_102_send_special(dali_master_handle_t handle, uint8_t special_byte, uint8_t data,
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

/**
 * @brief Set the 24-bit Part 102 search address (H/M/L bytes).
 */
static esp_err_t dali_102_set_search_addr(dali_master_handle_t handle, uint32_t addr, int tx_timeout_ms)
{
    esp_err_t err;
    err = dali_102_send_special(handle, DALI_SPECIAL_SEARCH_ADDR_H, (uint8_t)((addr >> 16) & 0xFFU),
                                false, tx_timeout_ms, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = dali_102_send_special(handle, DALI_SPECIAL_SEARCH_ADDR_M, (uint8_t)((addr >> 8) & 0xFFU),
                                false, tx_timeout_ms, NULL);
    if (err != ESP_OK) {
        return err;
    }
    return dali_102_send_special(handle, DALI_SPECIAL_SEARCH_ADDR_L, (uint8_t)(addr & 0xFFU),
                                 false, tx_timeout_ms, NULL);
}

/**
 * @brief Issue a COMPARE command and report whether any device replied YES.
 */
static esp_err_t dali_102_compare(dali_master_handle_t handle, int tx_timeout_ms, bool *yes_out)
{
    int result = DALI_RESULT_NO_REPLY;
    esp_err_t err = dali_102_send_special(handle, DALI_SPECIAL_COMPARE, 0x00U, false, tx_timeout_ms, &result);
    if (err != ESP_OK) {
        return err;
    }
    *yes_out = (result == 0xFF);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

esp_err_t dali_commission(dali_master_handle_t handle, dali_commission_mode_t mode, uint8_t start_addr,
                          uint8_t max_devices, uint8_t *count, int tx_timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle must not be NULL");
    ESP_RETURN_ON_FALSE(max_devices > 0 && max_devices <= 64, ESP_ERR_INVALID_ARG, TAG, "max_devices must be 1–64");
    ESP_RETURN_ON_FALSE((uint32_t)start_addr + max_devices <= 64, ESP_ERR_INVALID_ARG, TAG, "address range exceeds 0–63");

    uint8_t found = 0;

#if CONFIG_DALI_PART103_ENABLED
    /* Put Part 103 input devices into quiescent mode while commissioning gear.
     * DLS-203-P can otherwise emit/input-device frames during Part 102 COMPARE
     * windows, which look like extra devices in the binary-search pool. */
    ESP_RETURN_ON_ERROR(dali_103_do_device_command(handle, DALI_ADDR_BROADCAST, 0, DALI_103_START_QUIESCENT_MODE,
                                                   true, tx_timeout_ms, NULL), TAG,
                        "[102 commission] Part103 START_QUIESCENT_MODE failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Step 0: Send Part 103 TERMINATE first, then wait for devices to settle.
     * A Part 103 input device in commissioning mode will respond to COMPARE on
     * the shared bus, causing it to appear as a phantom 102 gear in the binary
     * search.  TERMINATE removes it from the pool; the settle delay ensures the
     * device has fully exited commissioning mode before we start the Part 102
     * INITIALIZE sequence. */
    ESP_RETURN_ON_ERROR(dali_103_send_special(handle, DALI_103_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL),
                        TAG, "[102 commission] Part103 TERMINATE failed");
    vTaskDelay(pdMS_TO_TICKS(50));
#endif /* CONFIG_DALI_PART103_ENABLED */

    /* Step 1: TERMINATE — abort any ongoing Part 102 commissioning session. */
    ESP_RETURN_ON_ERROR(dali_102_send_special(handle, DALI_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL), TAG,
                        "[102 commission] TERMINATE failed");

    /* Step 2: INITIALISE (send-twice).
     *   0x00 → all gear enter commissioning mode (DALI_COMMISSION_ALL)
     *   0xFF → only gear without a short address (DALI_COMMISSION_UNADDRESSED) */
    uint8_t init_byte = (mode == DALI_COMMISSION_UNADDRESSED) ? 0xFFU : 0x00U;
    ESP_RETURN_ON_ERROR(dali_102_send_special(handle, DALI_SPECIAL_INITIALIZE, init_byte, true, tx_timeout_ms, NULL),
                        TAG, "[102 commission] INITIALISE failed");

#if CONFIG_DALI_PART103_ENABLED
    /* Step 2b: DLS-203-P can enter/keep a Part 103 addressing state when it sees
     * the Part 102 INITIALISE sequence.  Expel Part 103 devices again before
     * RANDOMISE so they cannot generate a random address and answer 102 COMPARE. */
    ESP_RETURN_ON_ERROR(dali_103_send_special(handle, DALI_103_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL),
                        TAG, "[102 commission] Part103 TERMINATE(post-init) failed");
    vTaskDelay(pdMS_TO_TICKS(50));
#endif /* CONFIG_DALI_PART103_ENABLED */

    /* Step 3: RANDOMISE (send-twice) — each gear generates a new 24-bit random address.
     * IEC 62386-102 requires at least DALI_RANDOMISE_SETTLE_MS after RANDOMISE
     * for all gear to complete generation. */
    ESP_RETURN_ON_ERROR(dali_102_send_special(handle, DALI_SPECIAL_RANDOMIZE, 0x00U, true, tx_timeout_ms, NULL), TAG,
                        "[102 commission] RANDOMISE failed");
    vTaskDelay(pdMS_TO_TICKS(DALI_RANDOMISE_SETTLE_MS));

    /* Step 4: Binary-search loop (IEC 62386-102 §11.3). */
    uint8_t next_addr = start_addr;
    ESP_LOGI(TAG, "[102 commission] Step4: binary search start (start_addr=%d, max_devices=%d)", start_addr, max_devices);

    while (next_addr < (uint8_t)(start_addr + max_devices)) {
        ESP_RETURN_ON_ERROR(dali_102_set_search_addr(handle, 0xFFFFFFUL, tx_timeout_ms), TAG,
                            "[102 commission] SET_SEARCH_ADDR(max) failed");
        bool yes = false;
        ESP_RETURN_ON_ERROR(dali_102_compare(handle, tx_timeout_ms, &yes), TAG, "[102 commission] COMPARE(max) failed");

        ESP_LOGI(TAG, "[102 commission] COMPARE(0xFFFFFF) → %s (slot %d)",
                 yes ? "YES (device found)" : "NO (no more devices)", next_addr);

        if (!yes) {
            break;
        }

        uint32_t low  = 0x000000UL;
        uint32_t high = 0xFFFFFFUL;

        while (low < high) {
            uint32_t mid = low + (high - low) / 2U;

            ESP_RETURN_ON_ERROR(dali_102_set_search_addr(handle, mid, tx_timeout_ms), TAG,
                                "[102 commission] SET_SEARCH_ADDR(bisect) failed");
            ESP_RETURN_ON_ERROR(dali_102_compare(handle, tx_timeout_ms, &yes), TAG,
                                "[102 commission] COMPARE(bisect) failed");

            if (yes) {
                high = mid;
            } else {
                low  = mid + 1U;
            }
        }

        ESP_RETURN_ON_ERROR(dali_102_set_search_addr(handle, low, tx_timeout_ms), TAG,
                            "[102 commission] SET_SEARCH_ADDR(final) failed");

        /* PROGRAM_SHORT_ADDR encoding: (short_addr << 1) | 0x01.
         * Address-changing special commands are sent twice for compatibility. */
        uint8_t encoded = (uint8_t)((next_addr << 1) | 0x01U);
        ESP_RETURN_ON_ERROR(dali_102_send_special(handle, DALI_SPECIAL_PROGRAM_SHORT_ADDR, encoded, true, tx_timeout_ms,
                                                  NULL), TAG, "[102 commission] PROGRAM_SHORT_ADDR failed");

        ESP_LOGI(TAG, "[102 commission] Assigned short address %d (random=0x%06"PRIX32")", next_addr, low);

        ESP_RETURN_ON_ERROR(dali_102_send_special(handle, DALI_SPECIAL_WITHDRAW, 0x00U, true, tx_timeout_ms, NULL),
                            TAG, "[102 commission] WITHDRAW failed");

        next_addr++;
        found++;
    }

    /* Step 5: TERMINATE — end the commissioning session for all gear. */
    ESP_RETURN_ON_ERROR(dali_102_send_special(handle, DALI_SPECIAL_TERMINATE, 0x00U, false, tx_timeout_ms, NULL),
                        TAG, "[102 commission] TERMINATE(final) failed");

#if CONFIG_DALI_PART103_ENABLED
    /* Keep Part 103 input devices in quiescent mode after commissioning.
     * The caller decides when to re-enable event reporting by issuing
     * STOP_QUIESCENT_MODE.  Leaving events enabled here would cause
     * sensor frames to appear on the bus immediately, which some Part 102
     * gear mis-decode as DAPC brightness commands. */
    ESP_RETURN_ON_ERROR(dali_103_do_device_command(handle, DALI_ADDR_BROADCAST, 0, DALI_103_START_QUIESCENT_MODE,
                                                   true, tx_timeout_ms, NULL), TAG,
                        "[102 commission] Part103 START_QUIESCENT_MODE failed");
    vTaskDelay(pdMS_TO_TICKS(50));
#endif /* CONFIG_DALI_PART103_ENABLED */

    ESP_LOGI(TAG, "[102 commission] Complete: %d gear addressed (start_addr=%d)", found, start_addr);

    if (count != NULL) {
        *count = found;
    }
    return ESP_OK;
}
