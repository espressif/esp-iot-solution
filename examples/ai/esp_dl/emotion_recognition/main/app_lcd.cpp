/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_lcd.hpp"
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_lcd_mipi_dsi.h"
#endif

esp_lcd_panel_handle_t app_lcd_init(void)
{
    esp_lcd_panel_handle_t panel = nullptr;

    /* Frames are drawn straight to the panel (no LVGL); we only need the handle. */
#if CONFIG_IDF_TARGET_ESP32P4
    bsp_display_config_t hw = {
        .dsi_bus = {
            .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        }
    };
    esp_lcd_panel_io_handle_t io = nullptr;
    ESP_ERROR_CHECK(bsp_display_new(&hw, &panel, &io));
#else /* ESP32-S31-Korvo: RGB panel */
    bsp_display_config_t hw = BSP_DISPLAY_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(bsp_display_new(&hw, &panel));
#endif

    bsp_display_brightness_set(100);
    bsp_display_backlight_on();
    return panel;
}
