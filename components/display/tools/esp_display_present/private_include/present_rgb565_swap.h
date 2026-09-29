/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RGB565 panel byte-order swap. Pixels are processed as 32-bit pairs so the
 * swap costs one load/store per two pixels; head/tail pixels handle 2-byte
 * aligned buffers.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint32_t __attribute__((may_alias)) present_rgb565_pair_t;

static inline uint16_t present_rgb565_swap_one(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static inline uint32_t present_rgb565_swap_pair(uint32_t value)
{
#if defined(__riscv_zbb)
    value = __builtin_bswap32(value);
    return (value >> 16) | (value << 16);
#else
    uint32_t low_bytes = 0x00ff00ffU;
#if defined(__XTENSA__)
    /* Hide the mask from GCC's bswap matcher: its rotate lowering needs
     * SAR reloads and is slower than the plain shift/mask form. */
    __asm__("" : "+r"(low_bytes));
#endif
    return ((value & low_bytes) << 8) | ((value >> 8) & low_bytes);
#endif
}

/** Swap @p count RGB565 pixels in place. @p pixels must be 2-byte aligned. */
static inline void present_rgb565_swap_in_place(uint16_t *pixels, size_t count)
{
    if (count != 0 && ((uintptr_t)pixels & 2U) != 0) {
        *pixels = present_rgb565_swap_one(*pixels);
        ++pixels;
        --count;
    }
    present_rgb565_pair_t *pairs = (present_rgb565_pair_t *)pixels;
    size_t pair_count = count / 2U;
    size_t index = 0;
    for (; index + 4U <= pair_count; index += 4U) {
        uint32_t p0 = pairs[index];
        uint32_t p1 = pairs[index + 1U];
        uint32_t p2 = pairs[index + 2U];
        uint32_t p3 = pairs[index + 3U];
        pairs[index] = present_rgb565_swap_pair(p0);
        pairs[index + 1U] = present_rgb565_swap_pair(p1);
        pairs[index + 2U] = present_rgb565_swap_pair(p2);
        pairs[index + 3U] = present_rgb565_swap_pair(p3);
    }
    for (; index < pair_count; ++index) {
        pairs[index] = present_rgb565_swap_pair(pairs[index]);
    }
    if ((count & 1U) != 0) {
        pixels[count - 1U] = present_rgb565_swap_one(pixels[count - 1U]);
    }
}

/**
 * Copy @p count RGB565 pixels from @p src to @p dst with the bytes of each
 * pixel swapped. Both pointers must be 2-byte aligned and must not overlap.
 */
static inline void present_rgb565_swap_copy(uint16_t *dst, const uint16_t *src,
                                            size_t count)
{
    if ((((uintptr_t)dst ^ (uintptr_t)src) & 2U) != 0) {
        for (size_t index = 0; index < count; ++index) {
            dst[index] = present_rgb565_swap_one(src[index]);
        }
        return;
    }
    if (count != 0 && ((uintptr_t)dst & 2U) != 0) {
        *dst++ = present_rgb565_swap_one(*src++);
        --count;
    }
    present_rgb565_pair_t *out = (present_rgb565_pair_t *)dst;
    const present_rgb565_pair_t *in = (const present_rgb565_pair_t *)src;
    size_t pair_count = count / 2U;
    size_t index = 0;
    for (; index + 4U <= pair_count; index += 4U) {
        uint32_t p0 = in[index];
        uint32_t p1 = in[index + 1U];
        uint32_t p2 = in[index + 2U];
        uint32_t p3 = in[index + 3U];
        out[index] = present_rgb565_swap_pair(p0);
        out[index + 1U] = present_rgb565_swap_pair(p1);
        out[index + 2U] = present_rgb565_swap_pair(p2);
        out[index + 3U] = present_rgb565_swap_pair(p3);
    }
    for (; index < pair_count; ++index) {
        out[index] = present_rgb565_swap_pair(in[index]);
    }
    if ((count & 1U) != 0) {
        dst[count - 1U] = present_rgb565_swap_one(src[count - 1U]);
    }
}
