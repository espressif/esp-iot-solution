/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dali.h"
#include "dali_command.h"

static const char *TAG = "dali_example";

#define STEP_DELAY_MS   1000
#define DALI_EXAMPLE_ADDR 0

static void example_dapc(dali_master_handle_t handle, const char *label, uint8_t level)
{
    ESP_LOGI(TAG, "[DAPC] %s", label);
    dali_master_transaction_config_t tx_cfg = {
        .addr_type = DALI_ADDR_SHORT,
        .addr = DALI_EXAMPLE_ADDR,
        .is_cmd = false,
        .command = level,
        .send_twice = false,
        .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
    };
    esp_err_t err = dali_master_do_transaction(handle, &tx_cfg, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DAPC failed: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
}

static void example_cmd(dali_master_handle_t handle, const char *label, uint8_t cmd, bool send_twice)
{
    ESP_LOGI(TAG, "[CMD]  %s%s", label, send_twice ? " (send-twice)" : "");
    dali_master_transaction_config_t tx_cfg = {
        .addr_type = DALI_ADDR_SHORT,
        .addr = DALI_EXAMPLE_ADDR,
        .is_cmd = true,
        .command = cmd,
        .send_twice = send_twice,
        .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
    };
    esp_err_t err = dali_master_do_transaction(handle, &tx_cfg, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Command failed: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS / 5));
}

static int example_query(dali_master_handle_t handle, const char *label, uint8_t cmd)
{
    int result = DALI_RESULT_NO_REPLY;
    dali_master_transaction_config_t tx_cfg = {
        .addr_type = DALI_ADDR_SHORT,
        .addr = DALI_EXAMPLE_ADDR,
        .is_cmd = true,
        .command = cmd,
        .send_twice = false,
        .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
    };
    esp_err_t err = dali_master_do_transaction(handle, &tx_cfg, &result);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Query failed: %s", esp_err_to_name(err));
    } else if (DALI_RESULT_IS_VALID(result)) {
        ESP_LOGI(TAG, "[QUERY] %-20s= 0x%02X  (%d)", label, (unsigned)result, result);
    } else {
        ESP_LOGW(TAG, "[QUERY] %-20s= NO REPLY", label);
    }
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS / 5));
    return result;
}

void app_main(void)
{
    /* Create a DALI driver instance — must be called once before any transaction. */
    dali_master_handle_t dali;
    dali_master_config_t cfg = {
        .rx_gpio = GPIO_NUM_5,
        .tx_gpio = GPIO_NUM_6,
        .invert_tx = false,
        .invert_rx = false,
    };
    dali_master_rmt_config_t rmt_cfg = {
        .mem_block_symbols = 64,
    };
    ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));

    uint32_t round = 0;

    while (true) {
        round++;
        ESP_LOGI(TAG, "=== Round %lu ===", (unsigned long)round);

        /* ------------------------------------------------------------------
         * Part 1: DAPC dimming sequence
         * Demonstrates direct arc-power control — the lamp visibly dims/brightens.
         * ------------------------------------------------------------------ */
        example_dapc(dali, "OFF",         0x00);
        example_dapc(dali, "25%  (0x3F)", 0x3F);
        example_dapc(dali, "50%  (0x7F)", 0x7F);
        example_dapc(dali, "MAX  (0xFE)", 0xFE);

        /* ------------------------------------------------------------------
         * Part 2: Indirect command — recall stored maximum level
         * ------------------------------------------------------------------ */
        example_cmd(dali, "RECALL_MAX_LEVEL", DALI_CMD_RECALL_MAX_LEVEL, false);

        /* ------------------------------------------------------------------
         * Part 3: Query commands — read back device status and levels
         * ------------------------------------------------------------------ */
        example_query(dali, "STATUS        ", DALI_CMD_QUERY_STATUS);
        example_query(dali, "ACTUAL_LEVEL  ", DALI_CMD_QUERY_ACTUAL_LEVEL);
        example_query(dali, "MAX_LEVEL     ", DALI_CMD_QUERY_MAX_LEVEL);
        example_query(dali, "MIN_LEVEL     ", DALI_CMD_QUERY_MIN_LEVEL);
        example_query(dali, "DEVICE_TYPE   ", DALI_CMD_QUERY_DEVICE_TYPE);

        /* ------------------------------------------------------------------
         * Part 4: Send-twice configuration command
         * STORE_ACTUAL_LEVEL saves the current output level as the new
         * power-on level.  It must be sent twice within 100 ms.
         * ------------------------------------------------------------------ */
        example_cmd(dali, "STORE_ACTUAL_LEVEL", DALI_CMD_STORE_ACTUAL_LEVEL, true);

        /* Pause before repeating */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
