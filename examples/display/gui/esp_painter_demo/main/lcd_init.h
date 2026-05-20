/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_LCD_H_RES         (1024)
#define EXAMPLE_LCD_V_RES         (600)
#define EXAMPLE_LCD_BIT_PER_PIXEL (16)
#define EXAMPLE_LCD_COLOR_SPACE   (LCD_RGB_ELEMENT_ORDER_RGB)

/**
 * @brief Initialize the MIPI DSI LCD panel (EK79007, 1024×600, RGB565).
 *
 * @param[out] panel            Returned panel handle
 * @param[out] io               Returned panel IO handle
 * @param[out] out_refresh_done Semaphore signaled by the ISR when draw_bitmap completes;
 *                              take this after each esp_lcd_panel_draw_bitmap call to wait
 *                              for the DMA transfer to finish before modifying the framebuffer.
 */
esp_err_t example_lcd_init(esp_lcd_panel_handle_t *panel,
                           esp_lcd_panel_io_handle_t *io,
                           SemaphoreHandle_t *out_refresh_done);

#ifdef __cplusplus
}
#endif
