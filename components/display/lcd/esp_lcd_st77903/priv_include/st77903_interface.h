/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_vendor.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialize ST77903 LCD panel with QSPI interface
 *
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t lcd_new_panel_st77903_qspi(const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

#if SOC_LCD_RGB_SUPPORTED
/**
 * @brief  Initialize ST77903 LCD panel with RGB interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t lcd_new_panel_st77903_rgb(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel);
#endif

#ifdef __cplusplus
}
#endif
