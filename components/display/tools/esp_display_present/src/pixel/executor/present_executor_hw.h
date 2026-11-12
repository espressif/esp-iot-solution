/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "pixel/present_pixel_ops.h"
#include "pixel/present_executor.h"
#include "platform/present_os.h"
#include "esp_cache.h"
#include "esp_private/dma2d.h"
/* Match the driver's extended destination rows, including quarter-turn height.
 * The allocation is exclusively borrowed; aligned edge lines remain inside it. */
static bool destination_band(const esp_display_present_copy_request_t *request, size_t alignment,
                             void **address, size_t *bytes)
{
    bool quarter = request->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || request->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
    size_t rows = quarter ? request->src_area.width : request->src_area.height;
    size_t stride = request->dst.layout.stride_bytes;
    size_t capacity = request->dst.capacity_bytes;
    if (!alignment || !stride || request->dst_y > capacity / stride) {
        return false;
    }
    size_t start = request->dst_y * stride;
    if (rows > (capacity - start) / stride) {
        return false;
    }
    size_t end = start + rows * stride;
    size_t padding = (alignment - end % alignment) % alignment;
    if (padding > capacity - end) {
        return false;
    }
    start -= start % alignment;
    end += padding;
    *address = (uint8_t *)request->dst.pixels + start;
    *bytes = end - start;
    return true;
}

static inline esp_err_t hw_sync(void *pixels, size_t bytes, int flags)
{
    esp_err_t e = esp_cache_msync(pixels, bytes, flags);
    return e == ESP_ERR_NOT_SUPPORTED ? ESP_OK : e;
}

/* Hardware reads complete source rows. Keep the request inside the declared
 * surface and let the cache API handle unaligned first/last lines instead of
 * extending into allocator padding that is not part of the surface contract. */
static inline esp_err_t hw_sync_source(const esp_display_present_copy_request_t *request)
{
    size_t stride = request->src.layout.stride_bytes;
    size_t start = request->src_area.y * stride;
    size_t bytes = request->src_area.height * stride;
    return hw_sync((uint8_t *)request->src.pixels + start, bytes,
                   ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}
static inline esp_err_t unsupported(esp_display_present_executor_reason_t *reason, esp_display_present_executor_reason_t why)
{
    *reason = why;
    return ESP_ERR_NOT_SUPPORTED;
}
static inline esp_err_t hw_probe(const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason,
                                 void **band, size_t *band_bytes)
{
    *reason = ESP_DISPLAY_PRESENT_EXEC_INVALID_REQUEST;
    esp_err_t e = esp_display_present_pixel_validate_request(request);
    if (e != ESP_OK) {
        return e;
    }
    size_t bpp = request->src.layout.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t sa = esp_cache_get_line_size_by_addr(request->src.pixels);
    size_t da = esp_cache_get_line_size_by_addr(request->dst.pixels);
    if (sa < 64) {
        sa = 64;
    }
    if (da < 64) {
        da = 64;
    }
    if (request->src.pixels == request->dst.pixels) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_IN_PLACE);
    }
    if ((uintptr_t)request->src.pixels % sa || (uintptr_t)request->dst.pixels % da) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_ALIGNMENT);
    }
    if (!request->dst.capacity_bytes || request->dst.capacity_bytes % da || request->dst.capacity_bytes > UINT32_MAX) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_CAPACITY);
    }
    if (request->src.layout.stride_bytes > UINT32_MAX || request->dst.layout.stride_bytes > UINT32_MAX ||
            request->src.layout.height > request->src.capacity_bytes / request->src.layout.stride_bytes ||
            request->dst.layout.height > request->dst.capacity_bytes / request->dst.layout.stride_bytes ||
            request->src.layout.stride_bytes / bpp > 0x3fff || request->src.layout.height > 0x3fff ||
            request->dst.layout.stride_bytes / bpp > 0x3fff || request->dst.layout.height > 0x3fff) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_LAYOUT);
    }
    if (!destination_band(request, da, band, band_bytes)) {
        return unsupported(reason, ESP_DISPLAY_PRESENT_EXEC_CAPACITY);
    }
    *reason = ESP_DISPLAY_PRESENT_EXEC_SUPPORTED;
    return ESP_OK;
}
