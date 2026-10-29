/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "esp_cache.h"
#include "esp_display_present_cache.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_cache_private.h"

size_t esp_display_present_cache_get_alignment(uint32_t caps, size_t fallback_alignment)
{
    size_t align = 0;
    if (esp_cache_get_alignment(caps, &align) != ESP_OK || align == 0) {
        return fallback_alignment;
    }
    return align;
}

size_t esp_display_present_get_cache_line_size_by_addr(const void *addr)
{
    if (!addr) {
        return 0;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    return esp_cache_get_line_size_by_addr(addr);
#else
    size_t align = 0;
    uint32_t caps = esp_ptr_external_ram(addr) ? MALLOC_CAP_SPIRAM :
                    esp_ptr_internal(addr)      ? MALLOC_CAP_INTERNAL : 0;
    if (caps == 0 || esp_cache_get_alignment(caps, &align) != ESP_OK) {
        return 0;
    }
    return align;
#endif
}

size_t esp_display_present_cache_get_aligned_size_by_addr(const void *addr, size_t size)
{
    size_t align = esp_display_present_get_cache_line_size_by_addr(addr);
    return (align > 0) ? (((size + align - 1) / align) * align) : size;
}

bool esp_display_present_cache_addr_aligned(const void *addr)
{
    size_t align = esp_display_present_get_cache_line_size_by_addr(addr);
    return (align == 0) || (((uintptr_t)addr & (align - 1)) == 0);
}

void esp_display_present_cache_msync_framebuffer(void *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return;
    }

    if (esp_display_present_get_cache_line_size_by_addr(buffer) == 0) {
        return;
    }

    esp_cache_msync(buffer, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

void esp_display_present_cache_invalidate_framebuffer(void *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return;
    }

    if (esp_display_present_get_cache_line_size_by_addr(buffer) == 0) {
        return;
    }

    esp_cache_msync(buffer, size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}
