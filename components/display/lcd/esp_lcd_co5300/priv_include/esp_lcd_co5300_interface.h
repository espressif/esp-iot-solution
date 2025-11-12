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
 * @brief Shared runtime context for CO5300 panel instances.
 *
 * Stored in `esp_lcd_panel_t::user_data` so transport-specific helpers can be
 * dispatched through the common public API.
 */
typedef struct {
    void *driver_data;
    esp_err_t (*apply_brightness)(void *driver_data, uint8_t brightness_percent);
} co5300_panel_context_t;

#if SOC_MIPI_DSI_SUPPORTED
/**
 * @brief  Initialize CO5300 LCD panel with MIPI interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_co5300_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                        esp_lcd_panel_handle_t *ret_panel);
#endif

esp_err_t esp_lcd_new_panel_co5300_spi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
