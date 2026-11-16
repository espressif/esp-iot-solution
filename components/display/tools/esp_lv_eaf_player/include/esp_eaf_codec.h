/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "eaf4.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frozen EAF codec ID table (see docs/NAMING.md) (0x00..0x06, zero gaps).
 *
 *   - 0x00..0x03 : Original EAF block codecs (RLE / Huffman / JPEG)
 *   - 0x04 LOSSLESS  : [Core 1] 100% Bit-Exact Lossless (565+A8 & BGRA8888, adaptive sub-modes)
 *   - 0x05 BOUNDED  : [Core 2] 4x4 Error-Bounded Blocks + Literal Escape (configured max error, 565 & BGRA8888)
 *   - 0x06 JPEG_DELTA : [Core 3] 16x16+8x8 Quadtree Delta Strip JPEG (HW/SW JPEG, native 565, no alpha)
 */
typedef enum {
    /* Original EAF block codec IDs (0x00 .. 0x03) */
    ESP_EAF_CODEC_RLE      = 0x00,
    ESP_EAF_CODEC_HUFFMAN_RLE = 0x01,
    ESP_EAF_CODEC_JPEG    = 0x02,
    ESP_EAF_CODEC_HUFFMAN     = 0x03,

    /* Animation codecs (0x04 .. 0x06) */
    ESP_EAF_CODEC_LOSSLESS         = 0x04,
    ESP_EAF_CODEC_BOUNDED         = 0x05,
    ESP_EAF_CODEC_JPEG_DELTA        = 0x06,
    ESP_EAF_CODEC_MAX              = 0x07,
} esp_eaf_codec_id_t;

typedef enum {
    ESP_EAF_CONTAINER_INVALID = 0,
    ESP_EAF_CONTAINER_EAF  = 1, /*!< 0x89 "EAF" / 0x89 "AAF" */
    ESP_EAF_CONTAINER_EAF4 = 2, /*!< "EAF4" with declared pixel format */
} esp_eaf_container_t;

typedef enum {
    ESP_EAF_OUT_RGB565   = 0, /*!< 16-bit RGB565 (+ optional A8 plane) */
    ESP_EAF_OUT_RGB888   = 1, /*!< Reserved for a direct 24-bit decoder backend. */
    ESP_EAF_OUT_BGRA8888 = 2, /*!< Direct BGRA8888 output where the payload/backend supports it. */
} esp_eaf_output_format_t;

static inline esp_eaf_container_t esp_eaf_container_probe(const void *data, size_t size)
{
    if (!data || size < 4) {
        return ESP_EAF_CONTAINER_INVALID;
    }
    const uint8_t *b = (const uint8_t *)data;
    if (b[0] == 0x89 && (memcmp(b + 1, "EAF", 3) == 0 || memcmp(b + 1, "AAF", 3) == 0)) {
        return ESP_EAF_CONTAINER_EAF;
    }
    if (memcmp(b, "EAF4", 4) == 0) {
        return ESP_EAF_CONTAINER_EAF4;
    }
    return ESP_EAF_CONTAINER_INVALID;
}

/* Canonical ID lookup only; this does not report enabled backend capabilities. */
const char *esp_eaf_codec_name(esp_eaf_codec_id_t codec_id);

static inline bool esp_eaf_codec_supports_alpha(esp_eaf_codec_id_t codec_id)
{
    return (codec_id >= ESP_EAF_CODEC_RLE && codec_id < ESP_EAF_CODEC_MAX &&
            codec_id != ESP_EAF_CODEC_JPEG &&
            codec_id != ESP_EAF_CODEC_JPEG_DELTA);
}

#ifdef __cplusplus
}
#endif
