/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

#include "esp_painter.h"

typedef struct {
    struct {
        uint32_t color;
    } brush;
    struct {
        uint16_t x_min;
        uint16_t y_min;
        uint16_t x_max;
        uint16_t y_max;
        uint32_t is_trans_background : 8;
        uint32_t color : 24;
    } canvas;
    uint8_t piexl_color_byte;
    const esp_painter_basic_font_t *default_font;
    esp_lcd_panel_handle_t lcd_panel;
    uint8_t *buffer;
    uint16_t buffer_stride;
} esp_painter_t;

static const char *TAG = "esp_painter";

static esp_err_t draw_point(esp_painter_handle_t handle, uint16_t x, uint16_t y, uint32_t color);

esp_err_t esp_painter_new(esp_painter_config_t *config, esp_painter_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(config && handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(config->piexl_color_byte > 0 && config->piexl_color_byte < 4, ESP_ERR_INVALID_ARG, TAG, "Byte of piexl color must be in [1, 3]");
    ESP_RETURN_ON_FALSE(config->brush.color < BIT(config->piexl_color_byte * 8), ESP_ERR_INVALID_ARG, TAG, "Brush color out of range");
    ESP_RETURN_ON_FALSE(config->canvas.width > 0 && config->canvas.height > 0, ESP_ERR_INVALID_ARG, TAG, "Canvas shape must > 0");
    ESP_RETURN_ON_FALSE(
        config->canvas.color == COLOR_CANVAS_TRANS_BG || config->canvas.color < BIT(config->piexl_color_byte * 8),
        ESP_ERR_INVALID_ARG, TAG, "Canvas color out of range"
    );
    ESP_RETURN_ON_FALSE(config->lcd_panel || config->canvas.buffer, ESP_ERR_INVALID_ARG, TAG, "LCD panel or canvas buffer must be set");
    if (config->canvas.buffer) {
        ESP_RETURN_ON_FALSE(config->canvas.stride >= config->canvas.width, ESP_ERR_INVALID_ARG, TAG, "Invalid canvas stride");
    }
    esp_painter_t *painter = (esp_painter_t *)malloc(sizeof(esp_painter_t));
    ESP_RETURN_ON_FALSE(painter, ESP_ERR_NO_MEM, TAG, "Malloc failed");

    painter->brush.color = config->brush.color;
    painter->canvas.x_min = config->canvas.x;
    painter->canvas.y_min = config->canvas.y;
    painter->canvas.x_max = config->canvas.x + config->canvas.width - 1;
    painter->canvas.y_max = config->canvas.y + config->canvas.height - 1;
    painter->canvas.is_trans_background = (config->canvas.color == COLOR_CANVAS_TRANS_BG) ? 1 : 0;
    painter->canvas.color = (painter->canvas.is_trans_background) ? 0 : config->canvas.color;
    painter->piexl_color_byte = config->piexl_color_byte;
    painter->default_font = config->default_font;
    painter->lcd_panel = config->lcd_panel;
    painter->buffer = (uint8_t *)config->canvas.buffer;
    painter->buffer_stride = config->canvas.stride;
    *handle = (void *)painter;

    return ESP_OK;
}

esp_err_t esp_painter_set_buffer(esp_painter_handle_t handle, void *buffer, uint16_t stride)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_painter_t *painter = (esp_painter_t *)handle;
    ESP_RETURN_ON_FALSE(buffer, ESP_ERR_INVALID_ARG, TAG, "Invalid buffer");
    ESP_RETURN_ON_FALSE(stride >= painter->canvas.x_max - painter->canvas.x_min + 1, ESP_ERR_INVALID_ARG, TAG, "Invalid buffer stride");
    painter->buffer = (uint8_t *)buffer;
    painter->buffer_stride = stride;
    return ESP_OK;
}

esp_err_t esp_painter_fill_rect(esp_painter_handle_t handle, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_painter_t *painter = (esp_painter_t *)handle;
    ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid rectangle size");

    x += painter->canvas.x_min;
    y += painter->canvas.y_min;
    ESP_RETURN_ON_FALSE(x <= painter->canvas.x_max && y <= painter->canvas.y_max, ESP_ERR_INVALID_ARG, TAG, "Invalid rectangle origin");
    color = (color == COLOR_BRUSH_DEFAULT) ? painter->brush.color : color;
    ESP_RETURN_ON_FALSE(color < BIT(painter->piexl_color_byte * 8), ESP_ERR_INVALID_ARG, TAG, "Brush color out of range");

    uint32_t x_end_calc = (uint32_t)x + width;
    uint32_t y_end_calc = (uint32_t)y + height;
    uint16_t x_end = x_end_calc > (uint32_t)painter->canvas.x_max + 1 ? painter->canvas.x_max + 1 : x_end_calc;
    uint16_t y_end = y_end_calc > (uint32_t)painter->canvas.y_max + 1 ? painter->canvas.y_max + 1 : y_end_calc;

    if (painter->buffer != NULL) {
        // Fill the memory canvas directly to avoid per-pixel LCD transactions.
        for (uint16_t row = y; row < y_end; row++) {
            uint8_t *pixel = painter->buffer + ((size_t)row * painter->buffer_stride + x) * painter->piexl_color_byte;
            for (uint16_t col = x; col < x_end; col++) {
                memcpy(pixel, &color, painter->piexl_color_byte);
                pixel += painter->piexl_color_byte;
            }
        }
        return ESP_OK;
    }

    for (uint16_t row = y; row < y_end; row++) {
        for (uint16_t col = x; col < x_end; col++) {
            ESP_RETURN_ON_ERROR(draw_point(handle, col, row, color), TAG, "Draw pixel error");
        }
    }
    return ESP_OK;
}

esp_err_t esp_painter_draw_char(esp_painter_handle_t handle, uint16_t x, uint16_t y, const esp_painter_basic_font_t *font, uint32_t color, char c)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_painter_t *painter = (esp_painter_t *)handle;
    ESP_RETURN_ON_FALSE(font || painter->default_font, ESP_ERR_INVALID_ARG, TAG, "Invalid font");
    font = (font) ? font : painter->default_font;
    ESP_RETURN_ON_FALSE(font->bitmap && font->width > 0 && font->height > 0, ESP_ERR_INVALID_ARG, TAG, "Font format error");
    ESP_RETURN_ON_FALSE(c >= ' ' && c <= '~', ESP_ERR_INVALID_ARG, TAG, "Invalid ASCII character");
    x += painter->canvas.x_min;
    y += painter->canvas.y_min;
    ESP_RETURN_ON_FALSE(x <= painter->canvas.x_max && y <= painter->canvas.y_max, ESP_ERR_INVALID_ARG, TAG, "Invalid x(%d) or y(%d) (out of canvas)", x, y);
    color = (color == COLOR_BRUSH_DEFAULT) ? painter->brush.color : color;
    ESP_RETURN_ON_FALSE(color < BIT(painter->piexl_color_byte * 8), ESP_ERR_INVALID_ARG, TAG, "Brush color out of range");

    uint16_t font_w = font->width;
    uint16_t c_size = font->height * ((font->width + 7) / 8);
    uint16_t c_offset = (c - ' ') * c_size;
    const uint8_t *p_c = &font->bitmap[c_offset];
    uint16_t x0 = x;
    uint8_t temp;
    for (int i = 0; i < c_size; i++) {
        temp = p_c[i];
        for (int j = 0; j < 8; j++) {
            if (temp & 0x80) {
                ESP_RETURN_ON_FALSE(y <= painter->canvas.y_max, ESP_ERR_INVALID_SIZE, TAG, "Y out of canvas");
                ESP_RETURN_ON_ERROR(draw_point(handle, x, y, color), TAG, "Draw pixel error");
            }
            temp <<= 1;
            x++;
            if (x == x0 + font_w) {
                x = x0;
                y++;
                break;
            }
        }
    }
    return ESP_OK;
}

esp_err_t esp_painter_draw_string(esp_painter_handle_t handle, uint16_t x, uint16_t y, const esp_painter_basic_font_t* font, uint32_t color, const char* text)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_painter_t *painter = (esp_painter_t *)handle;
    ESP_RETURN_ON_FALSE(font || painter->default_font, ESP_ERR_INVALID_ARG, TAG, "Invalid font");
    font = (font) ? font : painter->default_font;

    uint16_t font_w = font->width;
    uint16_t font_h = font->height;
    uint16_t x0 = x;
    while (*text != 0) {
        if (*text == '\n') {
            y += font_h;
            x = x0;
        } else {
            ESP_RETURN_ON_FALSE(y + font_h - 1 <= painter->canvas.y_max, ESP_ERR_INVALID_SIZE, TAG, "Text out ouf canvas");
            ESP_RETURN_ON_ERROR(esp_painter_draw_char(handle, x, y, font, color, *text), TAG, "Draw char failed");
        }
        x += font_w;
        if (x + font_w > painter->canvas.x_max - painter->canvas.x_min + 1) {
            y += font_h;
            x = x0;
        }
        text++;
    }
    return ESP_OK;
}

esp_err_t esp_painter_draw_string_format(
    esp_painter_handle_t handle, uint16_t x, uint16_t y, const esp_painter_basic_font_t *font, uint32_t color, const char *fmt, ...)
{
    char buffer[CONFIG_ESP_PAINTER_FORMAT_SIZE_MAX];
    va_list args;
    int ret;
    va_start(args, fmt);
    ret = vsnprintf(buffer, CONFIG_ESP_PAINTER_FORMAT_SIZE_MAX, fmt, args);
    va_end(args);
    ESP_RETURN_ON_FALSE(ret >= 0, ESP_FAIL, TAG, "Format error");

    ESP_RETURN_ON_ERROR(esp_painter_draw_string(handle, x, y, font, color, buffer), TAG, "Draw string failed");
    return ESP_OK;
}

static esp_err_t draw_point(esp_painter_handle_t handle, uint16_t x, uint16_t y, uint32_t color)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (painter->buffer != NULL) {
        uint8_t *pixel = painter->buffer + ((size_t)y * painter->buffer_stride + x) * painter->piexl_color_byte;
        memcpy(pixel, &color, painter->piexl_color_byte);
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(painter->lcd_panel, ESP_ERR_INVALID_STATE, TAG, "LCD panel not configured");
    return esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)painter->lcd_panel, x, y, x + 1, y + 1, &color);
}
