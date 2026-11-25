/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdlib.h>
#include "eaf_port.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_timer.h"
#include "miniz.h"
#define ESP_EAF_ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

/**
 * @brief Get cache alignment for PSRAM
 */
static inline size_t get_psram_cache_align(void)
{
    size_t cache_align = 0;
    esp_cache_get_alignment(0, &cache_align);
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    size_t external_align = 0;
    esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &external_align);
    if (external_align > cache_align) {
        cache_align = external_align;
    }
#endif
    /* Fallback to 4-byte alignment if cache alignment not available */
    if (cache_align == 0) {
        cache_align = 4;
    }
    return cache_align;
}

/**
 * @brief Allocate memory with PSRAM priority
 *
 * Tries to allocate from PSRAM first, falls back to internal RAM if unavailable.
 * This function prioritizes PSRAM usage to save SRAM.
 */
void *eaf_alloc(size_t size)
{
    void *ptr = NULL;

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

    /* Fallback to default memory (internal RAM) */
    if (ptr == NULL) {
        ptr = malloc(size);
    }

    return ptr;
}

/**
 * @brief Allocate aligned memory with PSRAM priority
 *
 * Tries to allocate aligned memory from PSRAM first, falls back to internal RAM if unavailable.
 * This is required for hardware JPEG decoder output buffers.
 *
 * @param size Size to allocate
 * @param alignment Alignment requirement (0 means use cache line alignment)
 * @param allocated_size Pointer to store actual allocated size (may be larger due to alignment)
 * @return Pointer to allocated memory, or NULL on failure
 */
void *eaf_alloc_aligned(size_t size, size_t alignment, size_t *allocated_size)
{
    void *ptr = NULL;
    size_t aligned_size = size;

    /* Use cache alignment if not specified */
    if (alignment == 0) {
        alignment = get_psram_cache_align();
    }

    /* Align size up to alignment boundary */
    aligned_size = ESP_EAF_ALIGN_UP(size, alignment);

    /* Try PSRAM first if available */
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    ptr = heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

    /* Fallback to default memory (internal RAM) with alignment */
    if (ptr == NULL) {
        ptr = heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_DEFAULT);
    }

    if (allocated_size != NULL) {
        *allocated_size = (ptr != NULL) ? aligned_size : 0;
    }

    return ptr;
}

void *eaf_port_alloc_internal(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
int64_t eaf_port_now_us(void)
{
    return esp_timer_get_time();
}
size_t eaf_port_inflate_raw(void *dst, size_t n, const void *src, size_t len)
{
    /* The ROM mem-to-mem helper puts its large decompressor on the task stack.
     * Keep that workspace off the stack so legacy Lossless works in 8 KiB tasks.
     * Each call owns its state, preserving independent/concurrent decoders. */
    if (!dst || !src) {
        return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
    }
    tinfl_decompressor *state = eaf_port_alloc_internal(sizeof(*state));
    if (!state) {
        state = eaf_alloc(sizeof(*state));
    }
    if (!state) {
        return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
    }
    tinfl_init(state);
    size_t input_size = len;
    size_t output_size = n;
    tinfl_status status = tinfl_decompress(state, src, &input_size, dst, dst,
                                           &output_size, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(state);
    return status == TINFL_STATUS_DONE ? output_size : TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
}
void eaf_port_sync(void *data, size_t size)
{
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
    size_t alignment = get_psram_cache_align();
    esp_cache_msync(data, ESP_EAF_ALIGN_UP(size, alignment), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
#else
    (void)data; (void)size;
#endif
}

void eaf_port_release_buffer(void *data, size_t size)
{
    if (!data) {
        return;
    }
    if (size && esp_ptr_external_ram(data)) {
        size_t line = get_psram_cache_align();
        esp_cache_msync(data, ESP_EAF_ALIGN_UP(size, line),
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    }
    free(data);
}

void eaf_port_call_once(eaf_once_t *once, void (*initialize)(void))
{
    if (atomic_load_explicit(&once->done, memory_order_acquire)) {
        return;
    }
    _lock_acquire(&once->lock);
    if (!atomic_load_explicit(&once->done, memory_order_relaxed)) {
        initialize();
        atomic_store_explicit(&once->done, true, memory_order_release);
    }
    _lock_release(&once->lock);
}
