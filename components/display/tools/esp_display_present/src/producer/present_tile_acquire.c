/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared tile acquire helpers for pool-backed modes.
 */

#include "present_mode_internal.h"

esp_err_t present_stage_drawbuf_borrow(
    const esp_display_present_drawbuf_pool_t *pool,
    uint8_t *next_drawbuf,
    size_t row_bytes,
    uint16_t requested_rows,
    void **out_pixels,
    uint16_t *out_rows)
{
    if (pool == NULL || next_drawbuf == NULL || pool->count == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (row_bytes == 0 || row_bytes > pool->bytes ||
            out_pixels == NULL || out_rows == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t rows = (uint16_t)(pool->bytes / row_bytes);
    if (rows > requested_rows) {
        rows = requested_rows;
    }
    uint8_t index = *next_drawbuf;
    *next_drawbuf = (uint8_t)((index + 1U) % pool->count);
    *out_pixels = pool->buffers[index];
    *out_rows = rows;
    return ESP_OK;
}

esp_err_t present_stage_acquire_tile(
    const esp_display_present_drawbuf_pool_t *pool,
    uint8_t *next_drawbuf,
    uint8_t color_bytes,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region)
{
    if (area == NULL || out_region == NULL || color_bytes == 0 ||
            area->x2 < area->x1 || area->y2 < area->y1) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t row_bytes =
        (size_t)(area->x2 - area->x1 + 1) * color_bytes;
    uint16_t requested_rows = (uint16_t)(area->y2 - area->y1 + 1);
    void *pixels = NULL;
    uint16_t rows = 0;
    esp_err_t ret = present_stage_drawbuf_borrow(
                        pool, next_drawbuf, row_bytes, requested_rows,
                        &pixels, &rows);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_region = (esp_display_presenter_region_t) {
        .surface = {
            .pixels = pixels,
            .stride_bytes = row_bytes,
            .width = (uint16_t)(area->x2 - area->x1 + 1),
            .height = rows,
            .pixel_format = pixel_format,
        },
        .origin_x = (uint16_t)area->x1,
        .origin_y = (uint16_t)area->y1,
    };
    return ESP_OK;
}
