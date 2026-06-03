/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>

#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_lcd_types.h"

#include "esp_painter_font.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PAINGER_FORMAT_SIZE_MAX (128)

/* Color definitions, RGB565 format */
#define COLOR_RGB565_BLACK          (0x0000)
#define COLOR_RGB565_NAVY           (0x000F)
#define COLOR_RGB565_DARKGREEN      (0x03E0)
#define COLOR_RGB565_DARKCYAN       (0x03EF)
#define COLOR_RGB565_MAROON         (0x7800)
#define COLOR_RGB565_PURPLE         (0x780F)
#define COLOR_RGB565_OLIVE          (0x7BE0)
#define COLOR_RGB565_LIGHTGREY      (0xC618)
#define COLOR_RGB565_DARKGREY       (0x7BEF)
#define COLOR_RGB565_BLUE           (0x001F)
#define COLOR_RGB565_GREEN          (0x07E0)
#define COLOR_RGB565_CYAN           (0x07FF)
#define COLOR_RGB565_RED            (0xF800)
#define COLOR_RGB565_MAGENTA        (0xF81F)
#define COLOR_RGB565_YELLOW         (0xFFE0)
#define COLOR_RGB565_WHITE          (0xFFFF)
#define COLOR_RGB565_ORANGE         (0xFD20)
#define COLOR_RGB565_GREENYELLOW    (0xAFE5)
#define COLOR_RGB565_PINK           (0xF81F)
#define COLOR_RGB565_SILVER         (0xC618)
#define COLOR_RGB565_GRAY           (0x8410)
#define COLOR_RGB565_LIME           (0x07E0)
#define COLOR_RGB565_TEAL           (0x0410)
#define COLOR_RGB565_FUCHSIA        (0xF81F)
#define COLOR_RGB565_ESP_BKGD       (0xD185)

#define COLOR_BRUSH_DEFAULT         BIT(24)
#define COLOR_CANVAS_TRANS_BG       BIT(25)

typedef void * esp_painter_handle_t;

typedef struct {
    struct {
        uint32_t color;
    } brush;
    struct {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
        uint32_t color;
    } canvas;
    uint8_t piexl_color_byte;
    const esp_painter_basic_font_t *default_font;
    esp_lcd_panel_handle_t lcd_panel;
} esp_painter_config_t;

esp_err_t esp_painter_new(esp_painter_config_t *config, esp_painter_handle_t *handle);

esp_err_t esp_painter_draw_char(esp_painter_handle_t handle, uint16_t x, uint16_t y, const esp_painter_basic_font_t *font, uint32_t color, char c);

esp_err_t esp_painter_draw_string(esp_painter_handle_t handle, uint16_t x, uint16_t y, const esp_painter_basic_font_t* font, uint32_t color, const char* text);

esp_err_t esp_painter_draw_string_format(esp_painter_handle_t handle, uint16_t x, uint16_t y, const esp_painter_basic_font_t* font, uint32_t color, const char* fmt, ...);

#ifdef __cplusplus
}
#endif
