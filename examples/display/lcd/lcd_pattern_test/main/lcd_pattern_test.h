/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*lcd_pattern_test_wait_cb_t)(void *user_ctx);

#define LCD_PATTERN_TEST_DEFAULT_CHUNK_LINES    (20)
#define LCD_PATTERN_TEST_DEFAULT_DELAY_MS       (2000)
#define LCD_PATTERN_TEST_DEFAULT_CHECKER_SIZE   (16)
#define LCD_PATTERN_TEST_DEFAULT_CYCLE_DELAY_MS (2000)

typedef struct {
    uint16_t h_res;
    uint16_t v_res;
    uint8_t bits_per_pixel;
    lcd_rgb_element_order_t color_space;
    uint16_t chunk_lines;
    uint32_t delay_ms;
    uint8_t checker_size;
} lcd_pattern_test_config_t;

// Note: Global callback state, intended for single-panel usage in this example.
void lcd_pattern_test_set_wait_callback(lcd_pattern_test_wait_cb_t wait_cb, void *wait_cb_ctx);

// Runs the full pattern sequence in an infinite loop.
void lcd_pattern_test_run(esp_lcd_panel_handle_t panel,
                          const lcd_pattern_test_config_t *config);

#ifdef __cplusplus
}
#endif
