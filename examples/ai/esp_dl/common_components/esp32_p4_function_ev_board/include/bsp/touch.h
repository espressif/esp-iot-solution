/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP touch configuration structure
 */
typedef struct {
    void *dummy;    /*!< Prepared for future use. */
} bsp_touch_config_t;

/**
 * @brief Create the touchscreen controller.
 *
 * @param[in]  config    touch configuration
 * @param[out] ret_touch esp_lcd_touch handle
 * @return ESP_OK on success, otherwise an esp_lcd_touch error code.
 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

/**
 * @brief Deinitialize touch.
 */
void bsp_touch_delete(void);

#ifdef __cplusplus
}
#endif
