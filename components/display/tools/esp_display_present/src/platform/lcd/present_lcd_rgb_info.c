/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "platform/lcd/present_lcd_rgb_info.h"
#ifdef PRESENT_HOST
#define PRESENT_RGB_SUPPORTED 1
#define PRESENT_RGB_LINK_SWITCH_SUPPORTED 1
#else
#include "soc/soc_caps.h"
#define PRESENT_RGB_SUPPORTED SOC_LCD_RGB_SUPPORTED
#if CONFIG_IDF_TARGET_ESP32S31
#include "hal/axi_dma_ll.h"
#define PRESENT_RGB_LINK_SWITCH_SUPPORTED AXI_DMA_LL_SUPPORT(TX_LINK_SWITCH)
#else
#define PRESENT_RGB_LINK_SWITCH_SUPPORTED 0
#endif
#endif
#if PRESENT_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

esp_err_t __attribute__((weak)) present_lcd_rgb_get_board_info(esp_lcd_panel_handle_t panel, present_lcd_rgb_info_t *info)
{
    (void)panel;
    (void)info;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t present_lcd_rgb_get_facts(esp_lcd_panel_handle_t panel, present_lcd_rgb_facts_t *facts)
{
    if (!panel || !facts) {
        return ESP_ERR_INVALID_ARG;
    }
    *facts = (present_lcd_rgb_facts_t) {
        0
    };
#if PRESENT_RGB_SUPPORTED && defined(ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO)
    esp_lcd_rgb_panel_frame_buffer_info_t info;
    esp_err_t error = esp_lcd_rgb_panel_get_frame_buffer_info(panel, &info);
    if (error != ESP_OK) {
        return error == ESP_ERR_NOT_FOUND ? ESP_ERR_NOT_SUPPORTED : error;
    }
    facts->panel = (present_lcd_rgb_info_t) {
        .width = info.width, .height = info.height,
        .frame_buffer_count = info.frame_buffer_count,
        .frame_buffer_bytes = info.frame_buffer_bytes,
        .bounce_buffer_bytes = info.bounce_buffer_bytes,
        .bits_per_pixel = info.bits_per_pixel,
        .continuous_refresh = info.continuous_refresh,
        .custom_bounce_callback = info.custom_bounce_callback,
        .transformed = info.transformed,
    };
    facts->retirement = info.frame_buffer_retirement && (info.bounce_buffer_bytes || PRESENT_RGB_LINK_SWITCH_SUPPORTED);
    return ESP_OK;
#else
    esp_err_t error = present_lcd_rgb_get_board_info(panel, &facts->panel);
    if (error == ESP_OK) {
        /* Chip capability is meaningful only after actual bounce state is
         * known. S3 non-bounce EOF is not a retirement guarantee. */
        facts->retirement = facts->panel.bounce_buffer_bytes || PRESENT_RGB_LINK_SWITCH_SUPPORTED;
    }
    return error;
#endif
}
