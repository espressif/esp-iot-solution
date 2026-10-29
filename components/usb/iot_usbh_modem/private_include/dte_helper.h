/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DTE_MAX_RETRIES CONFIG_MODEM_OPERATION_RETRY_TIMES
#define DTE_RETRY_DELAY CONFIG_MODEM_OPERATION_RETRY_DELAY_MS // milliseconds
#define DTE_RETRY_OPERATION(condition_expr, interrupt_flag) \
    ({                                                                                           \
        bool success = false;                                                                    \
        int retries = 0;                                                                         \
        while (retries < (DTE_MAX_RETRIES)) {                                                    \
            if (interrupt_flag) {                                                                \
                ESP_LOGW(TAG, "Retry interrupted by external flag.");                            \
                break;                                                                           \
            }                                                                                    \
            if (condition_expr) {                                                                \
                success = true;                                                                  \
                break;                                                                           \
            }                                                                                    \
            retries++;                                                                           \
            ESP_LOGW(TAG, "Retry %d/%d failed. Retrying in %d milliseconds...", retries, (DTE_MAX_RETRIES), (DTE_RETRY_DELAY)); \
            vTaskDelay(pdMS_TO_TICKS(DTE_RETRY_DELAY));                                          \
        }                                                                                        \
        if (!success) {                                                                          \
            ESP_LOGW(TAG, "Operation failed.");                                                  \
        }                                                                                        \
        success;                                                                                 \
    })
