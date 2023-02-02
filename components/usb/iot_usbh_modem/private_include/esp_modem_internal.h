/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "esp_modem.h"

/**
* @brief Macro defined for error checking
*
*/
#define ESP_MODEM_ERR_CHECK(a, str, goto_tag, ...)                                    \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)

/**
* @brief common modem delay function
*
*/
static inline void esp_modem_wait_ms(size_t time)
{
    vTaskDelay(pdMS_TO_TICKS(time));
}

#ifdef __cplusplus
}
#endif
