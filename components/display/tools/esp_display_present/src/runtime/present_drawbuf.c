/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present_drawbuf.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"

esp_err_t esp_display_present_drawbuf_pool_alloc(
    esp_display_present_drawbuf_pool_t *pool,
    uint16_t width,
    uint16_t lines,
    uint8_t color_bytes,
    uint8_t count,
    bool in_psram)
{
    if (pool == NULL || width == 0 || lines == 0 || color_bytes == 0 ||
            count == 0 || count > ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(pool, 0, sizeof(*pool));
    pool->bytes = (size_t)width * lines * color_bytes;
    pool->lines = lines;
    pool->count = count;

    uint32_t caps = in_psram
                    ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA)
                    : (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    for (uint8_t index = 0; index < count; ++index) {
        pool->buffers[index] = heap_caps_malloc(pool->bytes, caps);
        if (pool->buffers[index] == NULL && in_psram) {
            pool->buffers[index] = heap_caps_malloc(
                                       pool->bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        }
        if (pool->buffers[index] == NULL) {
            esp_display_present_drawbuf_pool_free(pool);
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

void esp_display_present_drawbuf_pool_free(
    esp_display_present_drawbuf_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }
    for (uint8_t index = 0;
            index < ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS; ++index) {
        free(pool->buffers[index]);
        pool->buffers[index] = NULL;
    }
    pool->bytes = 0;
    pool->lines = 0;
    pool->count = 0;
}
