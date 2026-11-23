/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RGB565 rotate/copy helpers adapted from esp_lvgl_adapter 0.7.9.
 * Kept local so display presentation does not depend on LVGL.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "present_rgb565_swap.h"

typedef uint16_t __attribute__((may_alias)) present_rgb565_half_t;

/* Destination is ascending and contiguous; source may walk backward or down a
 * column. Only form the next source pointer when more pixels remain. */
static inline void IRAM_ATTR present_rgb565_copy_run(present_rgb565_half_t *dst, const present_rgb565_half_t *src,
                                                     ptrdiff_t step, int count, bool swap)
{
    if ((uintptr_t)dst & 2U) {
        *dst++ = swap ? present_rgb565_swap_one(*src) : *src;
        if (--count == 0) {
            return;
        }
        src += step;
    }
    present_rgb565_pair_t *pairs = (present_rgb565_pair_t *)dst;
    if (swap) {
        while (count >= 2) {
            uint32_t pair = (uint32_t)src[0] | ((uint32_t)src[step] << 16);
            *pairs++ = present_rgb565_swap_pair(pair);
            count -= 2;
            if (count != 0) {
                src += step * 2;
            }
        }
    } else {
        while (count >= 2) {
            *pairs++ = (uint32_t)src[0] | ((uint32_t)src[step] << 16);
            count -= 2;
            if (count != 0) {
                src += step * 2;
            }
        }
    }
    if (count != 0) {
        *(present_rgb565_half_t *)pairs = swap ? present_rgb565_swap_one(*src) : *src;
    }
}

/* Source starts at the inclusive logical ROI; destination is a packed physical
 * framebuffer. The buffers must not overlap and the ROI/stride must be valid.
 * Byte swapping changes the two bytes within each pixel, not the pixel order. */
static inline void IRAM_ATTR present_rgb565_rotate_copy(const void *src, void *dst,
                                                        uint16_t xs, uint16_t ys,
                                                        uint16_t xe, uint16_t ye,
                                                        uint16_t src_stride_px,
                                                        uint16_t hor_res, uint16_t ver_res,
                                                        int rotation, int block_small,
                                                        int block_large, bool swap)
{
    const int w = xe - xs + 1, h = ye - ys + 1;
    const size_t source_stride = (size_t)src_stride_px * 2;
    const size_t destination_stride = (size_t)hor_res * 2;
    if (rotation == 0) {
        const uint8_t *in = src;
        uint8_t *out = (uint8_t *)dst + (size_t)ys * destination_stride + (size_t)xs * 2;
        if (!swap && w == src_stride_px && w == hor_res && xs == 0) {
            memcpy(out, in, (size_t)h * source_stride);
            return;
        }
        for (int y = 0; y < h; ++y) {
            if (swap) {
                present_rgb565_swap_copy((uint16_t *)out, (const uint16_t *)in, w);
            } else {
                memcpy(out, in, (size_t)w * 2);
            }
            if (y + 1 < h) {
                in += source_stride;
                out += destination_stride;
            }
        }
        return;
    }
    if ((((uintptr_t)src | (uintptr_t)dst) & 1U) == 0) {
        const present_rgb565_half_t *in = src;
        present_rgb565_half_t *out = dst;
        if (rotation == 180) {
            for (int y = 0; y < h; ++y) {
                present_rgb565_half_t *to = out + (size_t)(ver_res - 1 - ys - y) * hor_res + hor_res - 1 - xe;
                const present_rgb565_half_t *from = in + (size_t)y * src_stride_px + w - 1;
                present_rgb565_copy_run(to, from, -1, w, swap);
            }
            return;
        }
        const int bw = block_small > 0 ? block_small : 32;
        const int bh = block_large > 0 ? block_large : 64;
        for (int j = 0; j < w; j += bw) {
            const int x_end = j + bw < w ? j + bw : w;
            for (int i = 0; i < h; i += bh) {
                const int y_end = i + bh < h ? i + bh : h;
                for (int x = j; x < x_end; ++x) {
                    if (rotation == 90) {
                        present_rgb565_copy_run(out + (size_t)(xs + x) * hor_res + hor_res - ys - y_end,
                                                in + (size_t)(y_end - 1) * src_stride_px + x,
                                                -(ptrdiff_t)src_stride_px, y_end - i, swap);
                    } else {
                        present_rgb565_copy_run(out + (size_t)(ver_res - 1 - xs - x) * hor_res + ys + i,
                                                in + (size_t)i * src_stride_px + x,
                                                src_stride_px, y_end - i, swap);
                    }
                }
            }
        }
        return;
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int gx = xs + x, gy = ys + y;
            const int dx = rotation == 90 ? hor_res - 1 - gy : rotation == 180 ? hor_res - 1 - gx : gy;
            const int dy = rotation == 90 ? gx : rotation == 180 ? ver_res - 1 - gy : ver_res - 1 - gx;
            const uint8_t *from = (const uint8_t *)src + (size_t)y * source_stride + (size_t)x * 2;
            uint8_t *to = (uint8_t *)dst + (size_t)dy * destination_stride + (size_t)dx * 2;
            if (swap) {
                to[0] = from[1];
                to[1] = from[0];
            } else {
                memcpy(to, from, 2);
            }
        }
    }
}
