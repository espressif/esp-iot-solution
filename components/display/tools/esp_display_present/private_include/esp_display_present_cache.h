/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t esp_display_present_cache_get_alignment(uint32_t caps, size_t fallback_alignment);

size_t esp_display_present_get_cache_line_size_by_addr(const void *addr);

size_t esp_display_present_cache_get_aligned_size_by_addr(const void *addr, size_t size);

bool esp_display_present_cache_addr_aligned(const void *addr);

void esp_display_present_cache_msync_framebuffer(void *buffer, size_t size);

/** Invalidate CPU cache so DMA/PPA writes into @p buffer become CPU-visible. */
void esp_display_present_cache_invalidate_framebuffer(void *buffer, size_t size);

#ifdef __cplusplus
}
#endif
