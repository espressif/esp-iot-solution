/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_cache.h"
#include "esp_idf_version.h"
#include "esp_painter.h"

static const char *TAG = "esp_painter";

typedef struct {
    struct {
        uint16_t width;
        uint16_t height;
    } canvas;
    esp_painter_color_format_t color_format;
    const esp_painter_basic_font_t *default_font;
    struct {
        uint32_t swap_color_bytes: 1;
    } flags;
    size_t bpp;            // bytes per pixel, set once at init
    size_t canvas_stride;  // canvas.width * bpp, set once at init
} esp_painter_t;

// Convert a 0x00RRGGBB color to the painter's framebuffer format.
// RGB565: 16-bit value (optionally byte-swapped). RGB888: original 0x00RRGGBB value.
// Never returns ESP_PAINTER_COLOR_TRANSPARENT (0xFF000000U).
static uint32_t painter_get_color(const esp_painter_t *painter, uint32_t color)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;

    switch (painter->color_format) {
    case ESP_PAINTER_COLOR_FORMAT_RGB565: {
        uint32_t val = ESP_PAINTER_RGB565(r, g, b);
        if (painter->flags.swap_color_bytes) {
            val = ((val & 0xFFU) << 8) | ((val >> 8) & 0xFFU);
        }
        return val;
    }
    case ESP_PAINTER_COLOR_FORMAT_RGB888:
        return color; // 0x00RRGGBB; caller writes R→G→B byte order
    default:
        return 0;
    }
}

// Write back CPU-side changes to memory so downstream DMA/display hardware sees the update.
// Skipped automatically if the buffer is not backed by a data cache (e.g. IRAM).
static esp_err_t painter_cache_sync(const uint8_t *buffer, uint32_t buffer_size)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    // esp_cache_get_line_size_by_addr added in IDF 6.0: returns 0 for non-cacheable addresses
    if (esp_cache_get_line_size_by_addr(buffer) == 0) {
        return ESP_OK;
    }
    return esp_cache_msync((void *)buffer, buffer_size,
                           ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
#else
    // IDF 5.x: no pre-check API; msync returns ESP_ERR_INVALID_ARG for non-cacheable addresses
    esp_err_t ret = esp_cache_msync((void *)buffer, buffer_size,
                                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    return (ret == ESP_ERR_INVALID_ARG) ? ESP_OK : ret;
#endif
}

// Fill a rectangle with a pre-computed format-specific color value.
// Clips to canvas bounds. Writes the first row directly, then copies it for subsequent rows.
// No cache sync.
static esp_err_t painter_fill_rect_raw(const esp_painter_t *painter, uint8_t *buffer, uint32_t buffer_size,
                                       uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                       uint32_t color_value)
{
    if (w == 0 || h == 0 || x >= painter->canvas.width || y >= painter->canvas.height) {
        return ESP_OK;
    }
    uint16_t x_end = ((uint32_t)x + w > painter->canvas.width)  ? painter->canvas.width  : (uint16_t)(x + w);
    uint16_t y_end = ((uint32_t)y + h > painter->canvas.height) ? painter->canvas.height : (uint16_t)(y + h);
    uint16_t fill_w = x_end - x;
    size_t row_bytes = (size_t)fill_w * painter->bpp;

    size_t last_end = (size_t)(y_end - 1) * painter->canvas_stride + (size_t)x_end * painter->bpp;
    if (last_end > buffer_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *first_row = buffer + (size_t)y * painter->canvas_stride + (size_t)x * painter->bpp;

    if (painter->bpp == 2) {
        uint8_t lo = color_value & 0xFFU;
        uint8_t hi = (color_value >> 8) & 0xFFU;
        for (uint16_t col = 0; col < fill_w; col++) {
            first_row[col * 2]     = lo;
            first_row[col * 2 + 1] = hi;
        }
    } else {
        uint8_t r = (color_value >> 16) & 0xFFU;
        uint8_t g = (color_value >> 8)  & 0xFFU;
        uint8_t b =  color_value        & 0xFFU;
        for (uint16_t col = 0; col < fill_w; col++) {
            first_row[col * 3]     = r;
            first_row[col * 3 + 1] = g;
            first_row[col * 3 + 2] = b;
        }
    }

    for (uint16_t row = (uint16_t)(y + 1); row < y_end; row++) {
        memcpy(buffer + (size_t)row * painter->canvas_stride + (size_t)x * painter->bpp,
               first_row, row_bytes);
    }
    return ESP_OK;
}

// Fill a rectangle with a 0x00RRGGBB color. Converts to framebuffer format once. No cache sync.
static esp_err_t painter_fill_rect(const esp_painter_t *painter, uint8_t *buffer, uint32_t buffer_size,
                                   uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color)
{
    return painter_fill_rect_raw(painter, buffer, buffer_size, x, y, w, h,
                                 painter_get_color(painter, color));
}

// Draw a hollow rectangle using four filled strips. Converts color once. No cache sync.
static esp_err_t painter_draw_rect(const esp_painter_t *painter, uint8_t *buffer, uint32_t buffer_size,
                                   uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                   uint16_t line_width, uint32_t color)
{
    if (w == 0 || h == 0 || line_width == 0) {
        return ESP_OK;
    }

    uint32_t cv = painter_get_color(painter, color);

    if ((uint32_t)line_width * 2 >= w || (uint32_t)line_width * 2 >= h) {
        return painter_fill_rect_raw(painter, buffer, buffer_size, x, y, w, h, cv);
    }

    // All coordinates are within a valid rect, so intermediate sums fit in uint16_t:
    // y + h - lw <= canvas.height - 1, x + w - lw <= canvas.width - 1
    uint16_t lw = line_width;
    esp_err_t ret;
    if ((ret = painter_fill_rect_raw(painter, buffer, buffer_size,
                                     x, y, w, lw, cv)) != ESP_OK) {
        return ret;
    }
    if ((ret = painter_fill_rect_raw(painter, buffer, buffer_size,
                                     x, (uint16_t)(y + h - lw), w, lw, cv)) != ESP_OK) {
        return ret;
    }
    if ((ret = painter_fill_rect_raw(painter, buffer, buffer_size,
                                     x, (uint16_t)(y + lw), lw, (uint16_t)(h - 2 * lw), cv)) != ESP_OK) {
        return ret;
    }
    return painter_fill_rect_raw(painter, buffer, buffer_size,
                                 (uint16_t)(x + w - lw), (uint16_t)(y + lw), lw, (uint16_t)(h - 2 * lw), cv);
}

// Draw a line segment using Bresenham's algorithm.
// At each step, stamps a centered square of line_width×line_width.
// No cache sync.
static esp_err_t painter_draw_line(const esp_painter_t *painter, uint8_t *buffer, uint32_t buffer_size,
                                   uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                   uint16_t line_width, uint32_t color)
{
    if (line_width == 0) {
        return ESP_OK;
    }

    uint32_t cv = painter_get_color(painter, color);
    int half = (int)line_width / 2;

    int dx  =  abs((int)x1 - (int)x0);
    int dy  = -abs((int)y1 - (int)y0);
    int sx  = (x0 < x1) ? 1 : -1;
    int sy  = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int cx  = (int)x0;
    int cy  = (int)y0;

    while (1) {
        int px = cx - half;
        int py = cy - half;
        uint16_t rx = (px < 0) ? 0 : (uint16_t)px;
        uint16_t ry = (py < 0) ? 0 : (uint16_t)py;
        painter_fill_rect_raw(painter, buffer, buffer_size, rx, ry, line_width, line_width, cv);

        if (cx == (int)x1 && cy == (int)y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            cx += sx;
        }
        if (e2 <= dx) {
            err += dx;
            cy += sy;
        }
    }
    return ESP_OK;
}

// Draw a single printable ASCII character (0x20–0x7E) directly into the buffer.
// cv    = pre-computed format-specific foreground color (from painter_get_color).
// bg_cv = pre-computed background color, or ESP_PAINTER_COLOR_TRANSPARENT to skip bg fill.
// Characters outside 0x20–0x7E are silently skipped. Rendering is clipped to canvas bounds.
// No cache sync.
static esp_err_t painter_draw_char(const esp_painter_t *painter, uint8_t *buffer, uint32_t buffer_size,
                                   uint16_t x, uint16_t y, const esp_painter_basic_font_t *font,
                                   uint32_t cv, uint32_t bg_cv, char c)
{
    if ((uint8_t)c < 0x20 || (uint8_t)c > 0x7E) {
        return ESP_OK;
    }
    if (x >= painter->canvas.width || y >= painter->canvas.height) {
        return ESP_OK;
    }

    // Clip to canvas bounds before any writes
    int row_end  = (int)y + font->height;
    if (row_end > painter->canvas.height) {
        row_end = painter->canvas.height;
    }
    int col_end  = (int)x + font->width;
    if (col_end > painter->canvas.width) {
        col_end = painter->canvas.width;
    }
    int draw_rows = row_end - (int)y;
    int draw_cols = col_end - (int)x;

    size_t last_end = (size_t)(row_end - 1) * painter->canvas_stride + (size_t)col_end * painter->bpp;
    if (last_end > buffer_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (bg_cv != ESP_PAINTER_COLOR_TRANSPARENT) {
        painter_fill_rect_raw(painter, buffer, buffer_size, x, y, font->width, font->height, bg_cv);
    }

    int bytes_per_row = (font->width + 7) / 8;
    const uint8_t *bitmap = font->bitmap + (uint32_t)((uint8_t)c - 0x20) * font->height * bytes_per_row;

    if (painter->bpp == 2) {
        uint8_t lo = cv & 0xFFU;
        uint8_t hi = (cv >> 8) & 0xFFU;
        for (int dy = 0; dy < draw_rows; dy++) {
            const uint8_t *row_bmp = bitmap + dy * bytes_per_row;
            uint8_t *row_ptr = buffer + (size_t)(y + dy) * painter->canvas_stride + (size_t)x * 2;
            for (int dx = 0; dx < draw_cols; dx++) {
                if (row_bmp[dx >> 3] & (0x80U >> (dx & 7))) {
                    row_ptr[dx * 2]     = lo;
                    row_ptr[dx * 2 + 1] = hi;
                }
            }
        }
    } else {
        uint8_t r = (cv >> 16) & 0xFFU;
        uint8_t g = (cv >> 8)  & 0xFFU;
        uint8_t b =  cv        & 0xFFU;
        for (int dy = 0; dy < draw_rows; dy++) {
            const uint8_t *row_bmp = bitmap + dy * bytes_per_row;
            uint8_t *row_ptr = buffer + (size_t)(y + dy) * painter->canvas_stride + (size_t)x * 3;
            for (int dx = 0; dx < draw_cols; dx++) {
                if (row_bmp[dx >> 3] & (0x80U >> (dx & 7))) {
                    row_ptr[dx * 3]     = r;
                    row_ptr[dx * 3 + 1] = g;
                    row_ptr[dx * 3 + 2] = b;
                }
            }
        }
    }
    return ESP_OK;
}

// Render a string into the framebuffer, clipped to [x, clip_w) × [y, clip_h).
// Foreground and background colors are converted to format-specific values once before the loop.
// Handles \n (newline) and \r (carriage return). No cache sync.
static esp_err_t painter_draw_text(const esp_painter_t *painter, uint8_t *buffer, uint32_t buffer_size,
                                   uint16_t x, uint16_t y, uint16_t clip_w, uint16_t clip_h,
                                   const esp_painter_basic_font_t *font,
                                   uint32_t color, uint32_t bg_color, const char *text)
{
    uint32_t cv   = painter_get_color(painter, color);
    uint32_t bg_cv = (bg_color == ESP_PAINTER_COLOR_TRANSPARENT)
                     ? ESP_PAINTER_COLOR_TRANSPARENT
                     : painter_get_color(painter, bg_color);

    // uint32_t cursors to avoid uint16_t wrap on very large canvases
    uint32_t cursor_x = x;
    uint32_t cursor_y = y;
    esp_err_t ret = ESP_OK;

    while (*text) {
        if (cursor_y >= clip_h) {
            break;
        }
        switch (*text) {
        case '\n':
            cursor_x = x;
            cursor_y += font->height;
            break;
        case '\r':
            cursor_x = x;
            break;
        default:
            if (cursor_x + font->width <= clip_w) {
                ret = painter_draw_char(painter, buffer, buffer_size,
                                        (uint16_t)cursor_x, (uint16_t)cursor_y,
                                        font, cv, bg_cv, *text);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to draw char at (%" PRIu32 ",%" PRIu32 ")", cursor_x, cursor_y);
                    return ret;
                }
            }
            cursor_x += font->width;
            if (cursor_x >= clip_w) {
                cursor_x = x;
                cursor_y += font->height;
            }
            break;
        }
        text++;
    }
    return ret;
}

esp_err_t esp_painter_draw_string(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                  uint16_t x, uint16_t y, const esp_painter_basic_font_t *font,
                                  uint32_t color, uint32_t bg_color, const char *text)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!font) {
        font = painter->default_font;
        if (!font) {
            ESP_LOGE(TAG, "No font specified");
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (x >= painter->canvas.width || y >= painter->canvas.height) {
        ESP_LOGW(TAG, "Starting position out of bounds: (%d,%d)", x, y);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = painter_draw_text(painter, buffer, buffer_size,
                                      x, y, painter->canvas.width, painter->canvas.height,
                                      font, color, bg_color, text);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw text");
        return ret;
    }
    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_draw_string_format(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                         uint16_t x, uint16_t y, const esp_painter_basic_font_t *font,
                                         uint32_t color, uint32_t bg_color, const char *fmt, ...)
{
    char buf[CONFIG_ESP_PAINTER_FORMAT_SIZE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    return esp_painter_draw_string(handle, buffer, buffer_size, x, y, font, color, bg_color, buf);
}

esp_err_t esp_painter_draw_rect(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                uint16_t line_width, uint32_t color)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = painter_draw_rect(painter, buffer, buffer_size, x, y, w, h, line_width, color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw rect");
        return ret;
    }
    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_draw_fill_rect(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                     uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                     uint32_t color)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = painter_fill_rect(painter, buffer, buffer_size, x, y, w, h, color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to fill rect");
        return ret;
    }
    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_draw_line(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                uint16_t line_width, uint32_t color)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = painter_draw_line(painter, buffer, buffer_size, x0, y0, x1, y1, line_width, color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw line");
        return ret;
    }
    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_draw_point(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                 uint16_t x, uint16_t y, uint16_t radius, uint32_t color)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t cv   = painter_get_color(painter, color);
    uint16_t side = (uint16_t)(2 * radius + 1);
    uint16_t px   = (x >= radius) ? (uint16_t)(x - radius) : 0;
    uint16_t py   = (y >= radius) ? (uint16_t)(y - radius) : 0;

    esp_err_t ret = painter_fill_rect_raw(painter, buffer, buffer_size, px, py, side, side, cv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw point");
        return ret;
    }
    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_draw_detection_box(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                         uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                         const esp_painter_box_style_t *style, const char *label)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer || !style) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = painter_draw_rect(painter, buffer, buffer_size,
                                      x, y, w, h, style->line_width, style->box_color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw detection box");
        return ret;
    }

    if (label && *label) {
        const esp_painter_basic_font_t *f = style->font ? style->font : painter->default_font;
        if (f) {
            uint32_t lx = (uint32_t)x + style->line_width;
            uint32_t ly = (uint32_t)y + style->line_width;
            if (lx < painter->canvas.width && ly < painter->canvas.height) {
                uint16_t clip_r = ((uint32_t)x + w <= painter->canvas.width)  ? (uint16_t)(x + w) : painter->canvas.width;
                uint16_t clip_b = ((uint32_t)y + h <= painter->canvas.height) ? (uint16_t)(y + h) : painter->canvas.height;
                ret = painter_draw_text(painter, buffer, buffer_size,
                                        (uint16_t)lx, (uint16_t)ly, clip_r, clip_b,
                                        f, style->text_color, ESP_PAINTER_COLOR_TRANSPARENT, label);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to draw detection box label");
                    return ret;
                }
            }
        } else {
            ESP_LOGW(TAG, "No font for detection box label, skipping");
        }
    }

    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_clear(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                            uint32_t color)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = painter_fill_rect(painter, buffer, buffer_size,
                                      0, 0, painter->canvas.width, painter->canvas.height, color);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear canvas");
        return ret;
    }
    ret = painter_cache_sync(buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cache writeback failed");
    }
    return ret;
}

esp_err_t esp_painter_get_text_size(const esp_painter_basic_font_t *font, const char *text,
                                    uint16_t *out_w, uint16_t *out_h)
{
    if (!font || !text || !out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t max_w = 0;
    uint32_t cur_w = 0;
    uint32_t lines = 1;

    while (*text) {
        if (*text == '\n') {
            if (cur_w > max_w) {
                max_w = cur_w;
            }
            cur_w = 0;
            lines++;
        } else if (*text != '\r') {
            cur_w += font->width;
        }
        text++;
    }
    if (cur_w > max_w) {
        max_w = cur_w;
    }

    uint64_t total_h = (uint64_t)lines * font->height;
    *out_w = (max_w   > 0xFFFF) ? 0xFFFF : (uint16_t)max_w;
    *out_h = (total_h > 0xFFFF) ? 0xFFFF : (uint16_t)total_h;
    return ESP_OK;
}

esp_err_t esp_painter_init(const esp_painter_config_t *config, esp_painter_handle_t *handle)
{
    if (!config || !handle) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->canvas.width == 0 || config->canvas.height == 0) {
        ESP_LOGE(TAG, "Canvas dimensions must be non-zero");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->flags.swap_color_bytes && config->color_format == ESP_PAINTER_COLOR_FORMAT_RGB888) {
        ESP_LOGW(TAG, "swap_color_bytes has no effect for RGB888 format");
    }

    esp_painter_t *painter = (esp_painter_t *)calloc(1, sizeof(esp_painter_t));
    if (!painter) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    painter->canvas.width           = config->canvas.width;
    painter->canvas.height          = config->canvas.height;
    painter->color_format           = config->color_format;
    painter->default_font           = config->default_font;
    painter->flags.swap_color_bytes = config->flags.swap_color_bytes;
    painter->bpp                    = (config->color_format == ESP_PAINTER_COLOR_FORMAT_RGB888) ? 3 : 2;
    painter->canvas_stride          = (size_t)config->canvas.width * painter->bpp;

    *handle = painter;
    ESP_LOGI(TAG, "Painter initialized: %dx%d, format: %d",
             config->canvas.width, config->canvas.height, config->color_format);
    return ESP_OK;
}

esp_err_t esp_painter_deinit(esp_painter_handle_t handle)
{
    esp_painter_t *painter = (esp_painter_t *)handle;
    if (!painter) {
        return ESP_ERR_INVALID_ARG;
    }
    free(painter);
    return ESP_OK;
}
