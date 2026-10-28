/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define PJPG_MAGIC "_PJPG__"
#define PJPG_MAGIC_LEN 7
#define PJPG_HEADER_SIZE 22

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t rgb_jpeg_size;
    uint32_t alpha_jpeg_size;
} pjpg_info_t;

typedef enum {
    PJPG_RGB565_LE,
    PJPG_RGB565_BE,
    PJPG_ARGB8888,
} pjpg_output_format_t;

/* Header-only parsing is also used by LVGL's image-info callbacks. */
esp_err_t esp_lv_pjpg_parse_header(const uint8_t *data, size_t size, pjpg_info_t *info);
esp_err_t esp_lv_pjpg_validate_data(const uint8_t *data, size_t size, pjpg_info_t *info);

/* RGB565A8 uses stride bytes per RGB row and stride / 2 bytes per alpha row.
 * ARGB8888 uses the LVGL byte order B, G, R, A. The caller owns the output. */
esp_err_t esp_lv_pjpg_decode_sw(const uint8_t *data, size_t size, uint8_t *output,
                                size_t output_size, size_t stride, pjpg_output_format_t format);
