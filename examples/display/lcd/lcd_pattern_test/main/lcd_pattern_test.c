/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lcd_pattern_test.h"

static const char *TAG = "lcd_pattern_test";
static lcd_pattern_test_wait_cb_t s_wait_cb = NULL;
static void *s_wait_cb_ctx = NULL;

static inline void lcd_pattern_test_pack_color(uint8_t *pixel, uint8_t bpp,
                                               lcd_rgb_element_order_t order,
                                               uint8_t r, uint8_t g, uint8_t b)
{
    if (order == LCD_RGB_ELEMENT_ORDER_BGR) {
        uint8_t tmp = r;
        r = b;
        b = tmp;
    }

    if (bpp == 16) {
        uint16_t value = ((uint16_t)(r & 0xF8) << 8) |
                         ((uint16_t)(g & 0xFC) << 3) |
                         ((uint16_t)(b & 0xF8) >> 3);
        pixel[0] = value & 0xFF;
        pixel[1] = (value >> 8) & 0xFF;
    } else {
        if (bpp == 18) {
            r &= 0xFC;
            g &= 0xFC;
            b &= 0xFC;
        }
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
    }
}

static esp_err_t lcd_pattern_test_alloc_buffer(uint8_t **out_buf, size_t *out_lines,
                                               size_t line_bytes, uint16_t v_res, uint16_t chunk_lines)
{
    uint16_t lines = chunk_lines ? chunk_lines : LCD_PATTERN_TEST_DEFAULT_CHUNK_LINES;
    if (lines > v_res) {
        lines = v_res;
    }

    size_t size = line_bytes * lines;
    uint8_t *buf = NULL;

    buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    }
    if (!buf && lines > 1) {
        lines = 1;
        size = line_bytes;
        buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) {
            buf = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        }
    }
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "no mem for pattern buffer");

    *out_buf = buf;
    *out_lines = lines;
    return ESP_OK;
}

static void lcd_pattern_test_wait(void)
{
    if (s_wait_cb) {
        s_wait_cb(s_wait_cb_ctx);
    }
}

static esp_err_t lcd_pattern_test_draw_solid(esp_lcd_panel_handle_t panel,
                                             const lcd_pattern_test_config_t *config,
                                             uint8_t r, uint8_t g, uint8_t b)
{
    size_t bytes_per_pixel = (config->bits_per_pixel + 7) / 8;
    size_t line_bytes = config->h_res * bytes_per_pixel;
    size_t lines = 0;
    uint8_t *buf = NULL;
    ESP_RETURN_ON_ERROR(lcd_pattern_test_alloc_buffer(&buf, &lines, line_bytes,
                                                      config->v_res, config->chunk_lines),
                        TAG, "alloc buffer failed");

    for (size_t x = 0; x < config->h_res; x++) {
        lcd_pattern_test_pack_color(buf + x * bytes_per_pixel, config->bits_per_pixel,
                                    config->color_space, r, g, b);
    }
    for (size_t i = 1; i < lines; i++) {
        memcpy(buf + i * line_bytes, buf, line_bytes);
    }

    for (size_t y = 0; y < config->v_res; y += lines) {
        size_t draw_lines = (y + lines > config->v_res) ? (config->v_res - y) : lines;
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, y, config->h_res, y + draw_lines, buf),
                            TAG, "draw solid failed");
        lcd_pattern_test_wait();
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t lcd_pattern_test_draw_color_bars(esp_lcd_panel_handle_t panel,
                                                  const lcd_pattern_test_config_t *config)
{
    const uint8_t colors[8][3] = {
        {255, 255, 255}, // white
        {255, 255,   0}, // yellow
        {  0, 255, 255}, // cyan
        {  0, 255,   0}, // green
        {255,   0, 255}, // magenta
        {255,   0,   0}, // red
        {  0,   0, 255}, // blue
        {  0,   0,   0}, // black
    };

    size_t bytes_per_pixel = (config->bits_per_pixel + 7) / 8;
    size_t line_bytes = config->h_res * bytes_per_pixel;
    size_t lines = 0;
    uint8_t *buf = NULL;
    ESP_RETURN_ON_ERROR(lcd_pattern_test_alloc_buffer(&buf, &lines, line_bytes,
                                                      config->v_res, config->chunk_lines),
                        TAG, "alloc buffer failed");

    for (size_t x = 0; x < config->h_res; x++) {
        size_t bar = (x * 8) / config->h_res;
        if (bar >= 8) {
            bar = 7;
        }
        lcd_pattern_test_pack_color(buf + x * bytes_per_pixel, config->bits_per_pixel,
                                    config->color_space,
                                    colors[bar][0], colors[bar][1], colors[bar][2]);
    }
    for (size_t i = 1; i < lines; i++) {
        memcpy(buf + i * line_bytes, buf, line_bytes);
    }

    for (size_t y = 0; y < config->v_res; y += lines) {
        size_t draw_lines = (y + lines > config->v_res) ? (config->v_res - y) : lines;
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, y, config->h_res, y + draw_lines, buf),
                            TAG, "draw color bars failed");
        lcd_pattern_test_wait();
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t lcd_pattern_test_draw_gradient(esp_lcd_panel_handle_t panel,
                                                const lcd_pattern_test_config_t *config)
{
    size_t bytes_per_pixel = (config->bits_per_pixel + 7) / 8;
    size_t line_bytes = config->h_res * bytes_per_pixel;
    size_t lines = 0;
    uint8_t *buf = NULL;
    ESP_RETURN_ON_ERROR(lcd_pattern_test_alloc_buffer(&buf, &lines, line_bytes,
                                                      config->v_res, config->chunk_lines),
                        TAG, "alloc buffer failed");

    for (size_t x = 0; x < config->h_res; x++) {
        uint8_t val = (config->h_res > 1) ? (uint8_t)((x * 255U) / (config->h_res - 1)) : 0;
        lcd_pattern_test_pack_color(buf + x * bytes_per_pixel, config->bits_per_pixel,
                                    config->color_space, val, val, val);
    }
    for (size_t i = 1; i < lines; i++) {
        memcpy(buf + i * line_bytes, buf, line_bytes);
    }

    for (size_t y = 0; y < config->v_res; y += lines) {
        size_t draw_lines = (y + lines > config->v_res) ? (config->v_res - y) : lines;
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, y, config->h_res, y + draw_lines, buf),
                            TAG, "draw gradient failed");
        lcd_pattern_test_wait();
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t lcd_pattern_test_draw_rgb_gradient_bars(esp_lcd_panel_handle_t panel,
                                                         const lcd_pattern_test_config_t *config)
{
    size_t bytes_per_pixel = (config->bits_per_pixel + 7) / 8;
    size_t line_bytes = config->h_res * bytes_per_pixel;
    size_t lines = 0;
    uint8_t *buf = NULL;

    ESP_RETURN_ON_ERROR(lcd_pattern_test_alloc_buffer(&buf, &lines, line_bytes,
                                                      config->v_res, config->chunk_lines),
                        TAG, "alloc buffer failed");

    for (size_t y = 0; y < config->v_res; y += lines) {
        size_t draw_lines = (y + lines > config->v_res) ? (config->v_res - y) : lines;
        for (size_t line = 0; line < draw_lines; line++) {
            size_t row = y + line;
            uint8_t val = (config->v_res > 1) ? (uint8_t)((row * 255U) / (config->v_res - 1)) : 0;
            uint8_t *row_ptr = buf + line * line_bytes;
            for (size_t x = 0; x < config->h_res; x++) {
                size_t bar = (x * 3) / config->h_res;
                if (bar >= 3) {
                    bar = 2;
                }
                uint8_t r = (bar == 0) ? val : 0;
                uint8_t g = (bar == 1) ? val : 0;
                uint8_t b = (bar == 2) ? val : 0;
                lcd_pattern_test_pack_color(row_ptr + x * bytes_per_pixel, config->bits_per_pixel,
                                            config->color_space, r, g, b);
            }
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, y, config->h_res, y + draw_lines, buf),
                            TAG, "draw rgb gradient bars failed");
        lcd_pattern_test_wait();
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t lcd_pattern_test_draw_checkerboard(esp_lcd_panel_handle_t panel,
                                                    const lcd_pattern_test_config_t *config)
{
    size_t bytes_per_pixel = (config->bits_per_pixel + 7) / 8;
    size_t line_bytes = config->h_res * bytes_per_pixel;
    size_t lines = 0;
    uint8_t *buf = NULL;
    uint8_t checker_size = config->checker_size ? config->checker_size : LCD_PATTERN_TEST_DEFAULT_CHECKER_SIZE;

    ESP_RETURN_ON_ERROR(lcd_pattern_test_alloc_buffer(&buf, &lines, line_bytes,
                                                      config->v_res, config->chunk_lines),
                        TAG, "alloc buffer failed");

    for (size_t y = 0; y < config->v_res; y += lines) {
        size_t draw_lines = (y + lines > config->v_res) ? (config->v_res - y) : lines;
        for (size_t line = 0; line < draw_lines; line++) {
            size_t row = y + line;
            uint8_t *row_ptr = buf + line * line_bytes;
            for (size_t x = 0; x < config->h_res; x++) {
                uint8_t on = ((x / checker_size) + (row / checker_size)) & 0x1;
                uint8_t val = on ? 255 : 0;
                lcd_pattern_test_pack_color(row_ptr + x * bytes_per_pixel, config->bits_per_pixel,
                                            config->color_space, val, val, val);
            }
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, y, config->h_res, y + draw_lines, buf),
                            TAG, "draw checkerboard failed");
        lcd_pattern_test_wait();
    }

    free(buf);
    return ESP_OK;
}

void lcd_pattern_test_set_wait_callback(lcd_pattern_test_wait_cb_t wait_cb, void *wait_cb_ctx)
{
    s_wait_cb = wait_cb;
    s_wait_cb_ctx = wait_cb_ctx;
}

static esp_err_t lcd_pattern_test_run_once(esp_lcd_panel_handle_t panel,
                                           const lcd_pattern_test_config_t *config)
{
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_ARG, TAG, "panel is null");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(config->h_res && config->v_res, ESP_ERR_INVALID_ARG, TAG, "invalid resolution");

    if (config->bits_per_pixel != 16 && config->bits_per_pixel != 18 && config->bits_per_pixel != 24) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint32_t delay_ms = config->delay_ms ? config->delay_ms : LCD_PATTERN_TEST_DEFAULT_DELAY_MS;

    ESP_LOGI(TAG, "pattern: solid black");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_solid(panel, config, 0, 0, 0),
                        TAG, "draw black failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: solid white");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_solid(panel, config, 255, 255, 255),
                        TAG, "draw white failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: solid red");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_solid(panel, config, 255, 0, 0),
                        TAG, "draw red failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: solid green");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_solid(panel, config, 0, 255, 0),
                        TAG, "draw green failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: solid blue");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_solid(panel, config, 0, 0, 255),
                        TAG, "draw blue failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: solid gray");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_solid(panel, config, 128, 128, 128),
                        TAG, "draw gray failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: color bars");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_color_bars(panel, config),
                        TAG, "draw color bars failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: grayscale gradient");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_gradient(panel, config),
                        TAG, "draw gradient failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: RGB gradient bars (vertical)");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_rgb_gradient_bars(panel, config),
                        TAG, "draw rgb gradient bars failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ESP_LOGI(TAG, "pattern: checkerboard");
    ESP_RETURN_ON_ERROR(lcd_pattern_test_draw_checkerboard(panel, config),
                        TAG, "draw checkerboard failed");
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return ESP_OK;
}

void lcd_pattern_test_run(esp_lcd_panel_handle_t panel,
                          const lcd_pattern_test_config_t *config)
{
    while (true) {
        ESP_ERROR_CHECK(lcd_pattern_test_run_once(panel, config));
        vTaskDelay(pdMS_TO_TICKS(LCD_PATTERN_TEST_DEFAULT_CYCLE_DELAY_MS));
    }
}
