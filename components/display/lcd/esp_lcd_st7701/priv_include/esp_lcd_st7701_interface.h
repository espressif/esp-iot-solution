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

#define ST7701_CMD_SDIR             (0xC7)
#define ST7701_CMD_SS_BIT           (1 << 2)

#define ST7701_CMD_CND2BKxSEL       (0xFF)
#define ST7701_CMD_BKxSEL_BYTE0     (0x77)
#define ST7701_CMD_BKxSEL_BYTE1     (0x01)
#define ST7701_CMD_BKxSEL_BYTE2     (0x00)
#define ST7701_CMD_BKxSEL_BYTE3     (0x00)
#define ST7701_CMD_CN2_BIT          (1 << 4)
#define ST7701_CMD_BKxSEL_BK0       (0x00)

#if SOC_LCD_RGB_SUPPORTED
/**
 * @brief  Initialize ST7701 LCD panel with RGB interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st7701_rgb(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);
#endif

#if SOC_MIPI_DSI_SUPPORTED
/**
 * @brief  Initialize ST7701 LCD panel with MIPI interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st7701_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                        esp_lcd_panel_handle_t *ret_panel);
#endif

#ifdef __cplusplus
}
#endif
