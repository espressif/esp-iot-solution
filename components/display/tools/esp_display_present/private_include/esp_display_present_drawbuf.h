/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS 2U

typedef struct {
    /**
     * Buffer storage is reusable scratch. Once a buffer is submitted or
     * cancelled, its previous contents are undefined and must not be read by
     * the producer after the next acquire.
     */
    void *buffers[ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS];
    size_t bytes;
    uint16_t lines;
    uint8_t count;
} esp_display_present_drawbuf_pool_t;

esp_err_t esp_display_present_drawbuf_pool_alloc(
    esp_display_present_drawbuf_pool_t *pool,
    uint16_t width,
    uint16_t lines,
    uint8_t color_bytes,
    uint8_t count,
    bool in_psram);

void esp_display_present_drawbuf_pool_free(
    esp_display_present_drawbuf_pool_t *pool);

#ifdef __cplusplus
}
#endif
