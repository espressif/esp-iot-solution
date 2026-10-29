/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_painter_font.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Color format of the target framebuffer
 */
typedef enum {
    ESP_PAINTER_COLOR_FORMAT_RGB565,
    ESP_PAINTER_COLOR_FORMAT_RGB888,
} esp_painter_color_format_t;

/**
 * @defgroup esp_painter_colors Predefined color constants
 *
 * All color parameters in esp_painter use 24-bit RGB888 encoding: @c 0x00RRGGBB.
 * These constants can be used directly, or custom colors can be composed with
 * @c ESP_PAINTER_RGB888(r,g,b) or as a plain hex literal (e.g. @c 0xFF6600U).
 * The painter converts the value to the target framebuffer format internally.
 * @{
 */
#define ESP_PAINTER_COLOR_BLACK       0x000000U
#define ESP_PAINTER_COLOR_NAVY        0x000080U
#define ESP_PAINTER_COLOR_DARKGREEN   0x008000U
#define ESP_PAINTER_COLOR_DARKCYAN    0x008080U
#define ESP_PAINTER_COLOR_MAROON      0x800000U
#define ESP_PAINTER_COLOR_PURPLE      0x800080U
#define ESP_PAINTER_COLOR_OLIVE       0x808000U
#define ESP_PAINTER_COLOR_LIGHTGREY   0xC0C0C0U
#define ESP_PAINTER_COLOR_DARKGREY    0x808080U
#define ESP_PAINTER_COLOR_BLUE        0x0000FFU
#define ESP_PAINTER_COLOR_GREEN       0x00FF00U
#define ESP_PAINTER_COLOR_CYAN        0x00FFFFU
#define ESP_PAINTER_COLOR_RED         0xFF0000U
#define ESP_PAINTER_COLOR_MAGENTA     0xFF00FFU
#define ESP_PAINTER_COLOR_YELLOW      0xFFFF00U
#define ESP_PAINTER_COLOR_WHITE       0xFFFFFFU
#define ESP_PAINTER_COLOR_ORANGE      0xFFA500U
#define ESP_PAINTER_COLOR_GREENYELLOW 0xADFF2FU
#define ESP_PAINTER_COLOR_PINK        0xFFC0CBU
/** @} */

/**
 * @brief Sentinel value for transparent (no-fill) text background.
 *
 * Pass this as the @p bg_color argument to @c esp_painter_draw_string() or
 * @c esp_painter_draw_string_format() to skip background fill and draw only
 * the foreground text pixels.
 */
#define ESP_PAINTER_COLOR_TRANSPARENT 0xFF000000U

/**
 * @brief Compose a 24-bit color value from R, G, B components (0x00RRGGBB).
 *
 * Use this macro — or a plain hex literal — for all esp_painter color parameters.
 * The painter converts to the framebuffer format (RGB565 or RGB888) internally.
 */
#define ESP_PAINTER_RGB888(r, g, b) \
    ((((uint32_t)(r) & 0xFFU) << 16) | (((uint32_t)(g) & 0xFFU) << 8) | ((uint32_t)(b) & 0xFFU))

/**
 * @brief Compute a raw 16-bit RGB565 pixel value from R, G, B components.
 *
 * @note This is a low-level utility macro. It is @b not used as a color parameter
 *       in esp_painter draw functions, which always take 0x00RRGGBB encoding.
 */
#define ESP_PAINTER_RGB565(r, g, b) ((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | ((b) >> 3))

typedef void *esp_painter_handle_t;

/**
 * @brief Visual style for esp_painter_draw_detection_box()
 */
typedef struct {
    uint16_t line_width;                  /*!< Border thickness in pixels */
    uint32_t box_color;                   /*!< Border color (0x00RRGGBB) */
    const esp_painter_basic_font_t *font; /*!< Label font; NULL uses the painter's default font */
    uint32_t text_color;                  /*!< Label text color (0x00RRGGBB) */
} esp_painter_box_style_t;

/**
 * @brief Painter configuration
 */
typedef struct {
    struct {
        uint16_t width;   /*!< Canvas width in pixels; must be > 0 */
        uint16_t height;  /*!< Canvas height in pixels; must be > 0 */
    } canvas;
    esp_painter_color_format_t color_format;      /*!< Framebuffer color format */
    const esp_painter_basic_font_t *default_font; /*!< Default font used when NULL is passed to draw functions */
    struct {
        uint32_t swap_color_bytes: 1; /*!< Swap upper/lower bytes of each RGB565 pixel (for big-endian displays) */
    } flags;
} esp_painter_config_t;

/**
 * @brief Create a painter instance
 *
 * @param[in]  config  Painter configuration; canvas dimensions must be non-zero
 * @param[out] handle  Returned painter handle
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t esp_painter_init(const esp_painter_config_t *config, esp_painter_handle_t *handle);

/**
 * @brief Fill the entire canvas with a solid color
 *
 * Equivalent to calling @c esp_painter_draw_fill_rect over the full canvas.
 * Useful for clearing the framebuffer before a new frame.
 *
 * @param[in] color  Fill color (0x00RRGGBB)
 */
esp_err_t esp_painter_clear(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                            uint32_t color);

/**
 * @brief Draw a null-terminated string into a framebuffer
 *
 * @param[in] handle       Painter handle
 * @param[in] buffer       Framebuffer pointer
 * @param[in] buffer_size  Size of framebuffer in bytes
 * @param[in] x            X coordinate of the text origin (top-left)
 * @param[in] y            Y coordinate of the text origin (top-left)
 * @param[in] font         Font to use, or NULL to use the default font
 * @param[in] color        Text color (0x00RRGGBB)
 * @param[in] bg_color     Background fill color (0x00RRGGBB), or @c ESP_PAINTER_COLOR_TRANSPARENT to skip
 * @param[in] text         Null-terminated ASCII string
 * @return ESP_OK on success
 */
esp_err_t esp_painter_draw_string(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                  uint16_t x, uint16_t y, const esp_painter_basic_font_t *font,
                                  uint32_t color, uint32_t bg_color, const char *text);

/**
 * @brief Draw a printf-style formatted string into a framebuffer
 *
 * The internal format buffer size is controlled by @c CONFIG_ESP_PAINTER_FORMAT_SIZE_MAX.
 *
 * @param[in] color     Text color (0x00RRGGBB)
 * @param[in] bg_color  Background fill color (0x00RRGGBB), or @c ESP_PAINTER_COLOR_TRANSPARENT to skip
 */
esp_err_t esp_painter_draw_string_format(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                         uint16_t x, uint16_t y, const esp_painter_basic_font_t *font,
                                         uint32_t color, uint32_t bg_color, const char *fmt, ...);

/**
 * @brief Draw a hollow rectangle border
 *
 * @param[in] color  Border color (0x00RRGGBB)
 */
esp_err_t esp_painter_draw_rect(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                uint16_t line_width, uint32_t color);

/**
 * @brief Draw a solid filled rectangle
 *
 * @param[in] color  Fill color (0x00RRGGBB)
 */
esp_err_t esp_painter_draw_fill_rect(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                     uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                     uint32_t color);

/**
 * @brief Draw a line segment between two points
 *
 * Uses Bresenham's line algorithm. At each step a filled square of
 * @p line_width × @p line_width is stamped, producing a uniformly thick line
 * suitable for skeleton links, lane markings, and motion vectors.
 *
 * @param[in] x0         Start point X
 * @param[in] y0         Start point Y
 * @param[in] x1         End point X
 * @param[in] y1         End point Y
 * @param[in] line_width Stroke thickness in pixels; 0 is a no-op
 * @param[in] color      Line color (0x00RRGGBB)
 */
esp_err_t esp_painter_draw_line(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                uint16_t line_width, uint32_t color);

/**
 * @brief Draw a filled square marker centered at (x, y)
 *
 * Draws a solid square of side @c (2*radius+1) centered on the given point.
 * Suitable for keypoints, landmarks, and feature-point annotations where a
 * single pixel would be too small to see on a high-resolution display.
 *
 * @param[in] x       Center X coordinate
 * @param[in] y       Center Y coordinate
 * @param[in] radius  Half-side of the marker in pixels (side = 2*radius+1);
 *                    radius 0 draws a single pixel
 * @param[in] color   Fill color (0x00RRGGBB)
 */
esp_err_t esp_painter_draw_point(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                 uint16_t x, uint16_t y, uint16_t radius, uint32_t color);

/**
 * @brief Draw an object detection bounding box with an optional text label
 *
 * Draws a hollow rectangle and renders @p label at the top-left corner inside the box
 * (offset by @c style->line_width). If @p label is NULL or empty, only the border is drawn.
 * If @c style->font is NULL the painter's default font is used; if no font is available
 * the label is silently skipped.
 *
 * @param[in] style  Visual style (border width, colors, font). Must not be NULL.
 * @param[in] label  Null-terminated ASCII label, or NULL to skip
 */
esp_err_t esp_painter_draw_detection_box(esp_painter_handle_t handle, uint8_t *buffer, uint32_t buffer_size,
                                         uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                         const esp_painter_box_style_t *style, const char *label);

/**
 * @brief Measure the pixel dimensions of a string without drawing it
 *
 * Iterates the string counting characters and newlines to determine the pixel
 * bounding box the string would occupy with the given font. Useful for centering
 * or pre-allocating label areas.
 *
 * @param[in]  font   Font to measure with; must not be NULL
 * @param[in]  text   Null-terminated ASCII string; must not be NULL
 * @param[out] out_w  Pixel width of the widest line
 * @param[out] out_h  Total pixel height (number of lines × font height)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t esp_painter_get_text_size(const esp_painter_basic_font_t *font, const char *text,
                                    uint16_t *out_w, uint16_t *out_h);

/**
 * @brief Destroy a painter instance and free resources
 */
esp_err_t esp_painter_deinit(esp_painter_handle_t handle);

#ifdef __cplusplus
}
#endif
