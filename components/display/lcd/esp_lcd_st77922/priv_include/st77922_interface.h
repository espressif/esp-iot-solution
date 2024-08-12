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

#define LCD_OPCODE_WRITE_CMD        (0x02ULL)
#define LCD_OPCODE_READ_CMD         (0x0BULL)
#define LCD_OPCODE_WRITE_COLOR      (0x32ULL)

#define ST77922_PAGE_CMD1           (0xF0)
#define ST77922_PAGE_CMD2           (0xF1)
#define ST77922_PAGE_CMD3           (0xF2)
#define ST77922_PARAM_SET           (0x00)

/**
 * @brief  Initialize ST77922 LCD panel with SPI/QSPI interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st77922_general(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

#if SOC_LCD_RGB_SUPPORTED
/**
 * @brief  Initialize ST77922 LCD panel with RGB interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st77922_rgb(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);
#endif

#if SOC_MIPI_DSI_SUPPORTED
/**
 * @brief  Initialize ST77922 LCD panel with MIPI interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st77922_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                         esp_lcd_panel_handle_t *ret_panel);
#endif

#ifdef __cplusplus
}
#endif
