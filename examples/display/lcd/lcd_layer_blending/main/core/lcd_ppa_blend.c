/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "lvgl_private.h"
#include "ppa_blend.h"
#include "ppa_blend_private.h"
#include "lcd_ppa_blend_private.h"

#define LCD_PPA_BLEND_FRAME_BUFFER_NUMS_MAX   (2)

static const char *TAG = "lcd_ppa_blend";

static esp_lcd_panel_handle_t lcd = NULL;
static void *lcd_fb[LCD_PPA_BLEND_FRAME_BUFFER_NUMS_MAX] = { };
static void *lcd_last_buf = NULL;

static ppa_blend_out_layer_t ppa_out_layer = {
    .buffer_size = LCD_PPA_BLEND_OUT_H_RES * LCD_PPA_BLEND_OUT_V_RES * LCD_PPA_BLEND_OUT_COLOR_BITS / 8,
    .w = LCD_PPA_BLEND_OUT_H_RES,
    .h = LCD_PPA_BLEND_OUT_V_RES,
    .color_mode = (LCD_PPA_BLEND_OUT_COLOR_BITS == 16) ? PPA_BLEND_COLOR_MODE_RGB565 : PPA_BLEND_COLOR_MODE_RGB888,
};

#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
static void *lcd_next_buf = NULL;
static SemaphoreHandle_t lcd_vsync_sem = NULL;
#endif

esp_err_t lcd_ppa_blend_init(esp_lcd_panel_handle_t display_panel)
{
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(display_panel, LCD_PPA_BLEND_FRAME_BUFFER_NUMS, &lcd_fb[0], &lcd_fb[1]),
                        TAG, "Failed to get frame buffer");
    lcd_last_buf = lcd_fb[0];

#if LCD_PPA_BLEND_FRAME_BUFFER_NUMS < 2
    ppa_out_layer.buffer = lcd_last_buf;
    ESP_RETURN_ON_ERROR(ppa_blend_set_out_layer(&ppa_out_layer), TAG, "Failed to set ppa out layer");
#else
    ppa_out_layer.buffer = lcd_fb[1];
    ESP_RETURN_ON_ERROR(ppa_blend_set_out_layer(&ppa_out_layer), TAG, "Failed to set ppa out layer");
#endif

#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
    lcd_vsync_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(lcd_vsync_sem, ESP_ERR_NO_MEM, TAG, "Create lcd vsync semaphore failed");
#endif

    lcd = display_panel;

    return ESP_OK;
}

esp_err_t lcd_ppa_blend_refresh(void)
{
#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
    lcd_next_buf = (lcd_last_buf == lcd_fb[0] ? lcd_fb[1] : lcd_fb[0]);

    ppa_out_layer.buffer = lcd_last_buf;
    ESP_RETURN_ON_ERROR(ppa_blend_set_out_layer(&ppa_out_layer), TAG, "Failed to set ppa out layer");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(lcd, 0, 0, LCD_PPA_BLEND_OUT_H_RES, LCD_PPA_BLEND_OUT_V_RES, lcd_next_buf),
                        TAG, "Failed to draw bitmap");
    lcd_last_buf = lcd_next_buf;

    xSemaphoreTake(lcd_vsync_sem, 0);
    xSemaphoreTake(lcd_vsync_sem, portMAX_DELAY);
#endif

    return ESP_OK;
}

void lcd_ppa_blend_set_buffer(void *buf)
{
    ppa_out_layer.buffer = buf;
    ppa_blend_set_out_layer(&ppa_out_layer);
}

void* lcd_ppa_blend_get_buffer(void)
{
    return lcd_last_buf == lcd_fb[0] ? lcd_fb[1] : lcd_fb[0];
}

#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
bool lcd_ppa_on_lcd_vsync_cb(void)
{
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(lcd_vsync_sem, &need_yield);
    return (need_yield == pdTRUE);
}
#endif
