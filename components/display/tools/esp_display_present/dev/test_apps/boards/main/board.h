/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stddef.h>
#include "esp_lcd_panel_ops.h"
esp_err_t board_panel_create(esp_lcd_panel_handle_t *panel, size_t buffer_count);
/* Called only after the presenter has stopped and deleted the panel. */
esp_err_t board_resources_destroy(void);
#if defined(PRESENT_BOARD_S31_RGB)
#define BOARD_NAME "S31-Korvo stock RGB 800x480"
#define WIDTH 800
#define HEIGHT 480
#define SCANOUT_KIND ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB
#elif CONFIG_IDF_TARGET_ESP32S3
#define BOARD_NAME "S3-R16 RGB GC9503"
#define WIDTH 480
#define HEIGHT 480
#define SCANOUT_KIND ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB
#else
#define BOARD_NAME "P4-v3 MIPI EK79007"
#define WIDTH 1024
#define HEIGHT 600
#define SCANOUT_KIND ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI
#endif
