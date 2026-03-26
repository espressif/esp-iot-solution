/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "dali.h"
#include "dali_command.h"

static const char *TAG = "dali_example";


#define STEP_DELAY_MS   1000
#define DALI_EXAMPLE_ADDR 0

static void example_dapc(const char *label, uint8_t level)
{
    ESP_LOGI(TAG, "[DAPC] %s", label);
    esp_err_t err = dali_transaction(DALI_ADDR_SHORT,
                                     DALI_EXAMPLE_ADDR,
                                     false,   /* is_cmd = false → DAPC */
                                     level,
                                     false,   /* send_twice */
                                     DALI_TX_TIMEOUT_MS,
                                     NULL);   /* no reply expected */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DAPC failed: %s", esp_err_to_name(err));
    }
    dali_wait_between_frames();
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
}


static void example_cmd(const char *label, uint8_t cmd, bool send_twice)
{
    ESP_LOGI(TAG, "[CMD]  %s%s", label, send_twice ? " (send-twice)" : "");
    esp_err_t err = dali_transaction(DALI_ADDR_SHORT,
                                     DALI_EXAMPLE_ADDR,
                                     true,    /* is_cmd = true */
                                     cmd,
                                     send_twice,
                                     DALI_TX_TIMEOUT_MS,
                                     NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Command failed: %s", esp_err_to_name(err));
    }
    dali_wait_between_frames();
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS / 5));
}


static int example_query(const char *label, uint8_t cmd)
{
    int result = DALI_RESULT_NO_REPLY;
    esp_err_t err = dali_transaction(DALI_ADDR_SHORT,
                                     DALI_EXAMPLE_ADDR,
                                     true,
                                     cmd,
                                     false,
                                     DALI_TX_TIMEOUT_MS,
                                     &result);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Query failed: %s", esp_err_to_name(err));
    } else if (DALI_RESULT_IS_VALID(result)) {
        ESP_LOGI(TAG, "[QUERY] %-20s= 0x%02X  (%d)", label,
                 (unsigned)result, result);
    } else {
        ESP_LOGW(TAG, "[QUERY] %-20s= NO REPLY", label);
    }
    dali_wait_between_frames();
    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS / 5));
    return result;
}

void app_main(void)
{
    /* Initialise the DALI driver — must be called once before any transaction. */
    ESP_ERROR_CHECK(dali_init((gpio_num_t)CONFIG_DALI_RX_GPIO,
                              (gpio_num_t)CONFIG_DALI_TX_GPIO));

    uint32_t round = 0;

    while (true) {
        round++;
        ESP_LOGI(TAG, "=== Round %lu ===", (unsigned long)round);

        /* ------------------------------------------------------------------
         * Part 1: DAPC dimming sequence
         * Demonstrates direct arc-power control — the lamp visibly dims/brightens.
         * ------------------------------------------------------------------ */
        example_dapc("OFF",         0x00);
        example_dapc("25%  (0x3F)", 0x3F);
        example_dapc("50%  (0x7F)", 0x7F);
        example_dapc("MAX  (0xFE)", 0xFE);

        /* ------------------------------------------------------------------
         * Part 2: Indirect command — recall stored maximum level
         * ------------------------------------------------------------------ */
        example_cmd("RECALL_MAX_LEVEL", DALI_CMD_RECALL_MAX_LEVEL, false);

        /* ------------------------------------------------------------------
         * Part 3: Query commands — read back device status and levels
         * ------------------------------------------------------------------ */
        example_query("STATUS        ", DALI_CMD_QUERY_STATUS);
        example_query("ACTUAL_LEVEL  ", DALI_CMD_QUERY_ACTUAL_LEVEL);
        example_query("MAX_LEVEL     ", DALI_CMD_QUERY_MAX_LEVEL);
        example_query("MIN_LEVEL     ", DALI_CMD_QUERY_MIN_LEVEL);
        example_query("DEVICE_TYPE   ", DALI_CMD_QUERY_DEVICE_TYPE);

        /* ------------------------------------------------------------------
         * Part 4: Send-twice configuration command
         * STORE_ACTUAL_LEVEL saves the current output level as the new
         * power-on level.  It must be sent twice within 100 ms.
         * ------------------------------------------------------------------ */
        example_cmd("STORE_ACTUAL_LEVEL", DALI_CMD_STORE_ACTUAL_LEVEL, true);

        /* Pause before repeating */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
