/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_display_present_rotate.h"

void IRAM_ATTR esp_display_present_rotate_copy(
    const esp_display_present_rotate_copy_request_t *request)
{
    if (request == NULL || request->source.pixels == NULL ||
            request->destination.pixels == NULL ||
            request->source.color_bytes == 0 ||
            request->source.color_bytes != request->destination.color_bytes ||
            request->source.stride_bytes % request->source.color_bytes != 0 ||
            request->logical_area.x1 < request->source_origin.x ||
            request->logical_area.y1 < request->source_origin.y ||
            request->logical_area.x2 < request->logical_area.x1 ||
            request->logical_area.y2 < request->logical_area.y1 ||
            request->logical_area.x2 - request->source_origin.x >=
            request->source.width ||
            request->logical_area.y2 - request->source_origin.y >=
            request->source.height) {
        return;
    }

    uint16_t x_start = (uint16_t)request->logical_area.x1;
    uint16_t y_start = (uint16_t)request->logical_area.y1;
    const uint8_t *src = (const uint8_t *)request->source.pixels +
                         (size_t)(y_start - request->source_origin.y) *
                         request->source.stride_bytes +
                         (size_t)(x_start - request->source_origin.x) *
                         request->source.color_bytes;
    void *dst_fb = request->destination.pixels;
    uint16_t x_end = (uint16_t)request->logical_area.x2;
    uint16_t y_end = (uint16_t)request->logical_area.y2;
    uint16_t src_stride_px =
        (uint16_t)(request->source.stride_bytes / request->source.color_bytes);
    uint16_t hor_res = request->destination.width;
    uint16_t ver_res = request->destination.height;
    esp_display_present_rotation_t rotation = request->rotation;
    uint8_t color_bytes = request->source.color_bytes;
    const int block_size_small = 32;
    const int block_size_large = 64;
    const int rect_w = x_end - x_start + 1;
    const int rect_h = y_end - y_start + 1;
    const int phys_w = hor_res;

    if (rotation == ESP_DISPLAY_PRESENT_ROTATE_0) {
        const uint8_t *src_base = (const uint8_t *)src;
        uint8_t *dst_base = (uint8_t *)dst_fb + (size_t)(y_start * phys_w + x_start) * color_bytes;
        size_t row_bytes = rect_w * color_bytes;
        size_t dst_stride_bytes = phys_w * color_bytes;
        size_t src_stride_bytes = src_stride_px * color_bytes;

        bool is_contiguous = (src_stride_px == rect_w) && (rect_w == phys_w) && (x_start == 0);

        if (is_contiguous) {
            size_t total_bytes = rect_h * row_bytes;
            memcpy(dst_base, src_base, total_bytes);
        } else {
            for (int y = 0; y < rect_h; y++) {
                memcpy(dst_base, src_base, row_bytes);
                src_base += src_stride_bytes;
                dst_base += dst_stride_bytes;
            }
        }
        return;
    }

    if (rotation == ESP_DISPLAY_PRESENT_ROTATE_180) {
        for (int y = 0; y < rect_h; y++) {
            const uint8_t *src_row = (const uint8_t *)src + (size_t)y * src_stride_px * color_bytes;
            int dst_y = ver_res - 1 - (y_start + y);
            int dst_x_start = hor_res - 1 - x_end;
            uint8_t *dst_row = (uint8_t *)dst_fb + (size_t)(dst_y * phys_w + dst_x_start) * color_bytes;

            if (color_bytes == 2) {
                /* Walk the destination forward (reading the source row
                 * backward) so pixel pairs merge into 32-bit stores. */
                uint16_t *dst16 = (uint16_t *)dst_row;
                const uint16_t *src16 = (const uint16_t *)src_row + rect_w - 1;
                int n = rect_w;
                if (((uintptr_t)dst16 & 3U) != 0U && n > 0) {
                    *dst16++ = *src16--;
                    n--;
                }
                uint32_t *dst32 = (uint32_t *)dst16;
                for (; n >= 2; n -= 2) {
                    uint32_t lo = *src16--;
                    uint32_t hi = *src16--;
                    *dst32++ = lo | (hi << 16);
                }
                if (n != 0) {
                    *(uint16_t *)dst32 = *src16;
                }
            } else if (color_bytes == 4) {
                uint32_t *dst32 = (uint32_t *)(dst_row + (rect_w - 1) * 4);
                const uint32_t *src32 = (const uint32_t *)src_row;
                for (int x = 0; x < rect_w; x++) {
                    *dst32-- = *src32++;
                }
            } else {
                for (int x = 0; x < rect_w; x++) {
                    const uint8_t *src_pixel = src_row + (size_t)x * color_bytes;
                    uint8_t *dst_pixel = dst_row + (size_t)(rect_w - 1 - x) * color_bytes;
                    memcpy(dst_pixel, src_pixel, color_bytes);
                }
            }
        }
        return;
    }

    int block_w = (rotation == ESP_DISPLAY_PRESENT_ROTATE_90 || rotation == ESP_DISPLAY_PRESENT_ROTATE_270) ?
                  block_size_small : block_size_large;
    int block_h = (rotation == ESP_DISPLAY_PRESENT_ROTATE_90 || rotation == ESP_DISPLAY_PRESENT_ROTATE_270) ?
                  block_size_large : block_size_small;

    if (color_bytes == 2) {
        /* 90/270 transpose, RGB565: one source column becomes one
         * destination row, written sequentially with pixel pairs
         * merged into 32-bit stores. The source column walk is strided
         * — cheap for the SRAM bounce block, and kept cache-resident
         * for PSRAM sources by the tile loops. */
        const int src_stride = src_stride_px;
        for (int j = 0; j < rect_w; j += block_w) {
            int tile_x_end = (j + block_w > rect_w) ? rect_w : j + block_w;
            for (int i = 0; i < rect_h; i += block_h) {
                int tile_y_end = (i + block_h > rect_h) ? rect_h : i + block_h;
                int run = tile_y_end - i;
                for (int x = j; x < tile_x_end; x++) {
                    int gx = x_start + x;
                    const uint16_t *src_px;
                    uint16_t *dst_px;
                    int src_step;
                    if (rotation == ESP_DISPLAY_PRESENT_ROTATE_90) {
                        /* Destination columns ascend as source rows
                         * descend: start at the bottom of the tile. */
                        int dx = phys_w - 1 - (y_start + tile_y_end - 1);
                        dst_px = (uint16_t *)dst_fb + (size_t)gx * phys_w + dx;
                        src_px = (const uint16_t *)src + (size_t)(tile_y_end - 1) * src_stride + x;
                        src_step = -src_stride;
                    } else {
                        int dx = y_start + i;
                        dst_px = (uint16_t *)dst_fb + (size_t)(ver_res - 1 - gx) * phys_w + dx;
                        src_px = (const uint16_t *)src + (size_t)i * src_stride + x;
                        src_step = src_stride;
                    }
                    int n = run;
                    if (((uintptr_t)dst_px & 3U) != 0U && n > 0) {
                        *dst_px++ = *src_px;
                        src_px += src_step;
                        n--;
                    }
                    uint32_t *dst32 = (uint32_t *)dst_px;
                    for (; n >= 2; n -= 2) {
                        uint32_t lo = *src_px;
                        src_px += src_step;
                        uint32_t hi = *src_px;
                        src_px += src_step;
                        *dst32++ = lo | (hi << 16);
                    }
                    if (n != 0) {
                        *(uint16_t *)dst32 = *src_px;
                    }
                }
            }
        }
        return;
    }

    for (int i = 0; i < rect_h; i += block_h) {
        int max_height = (i + block_h > rect_h) ? rect_h : i + block_h;

        for (int j = 0; j < rect_w; j += block_w) {
            int max_width = (j + block_w > rect_w) ? rect_w : j + block_w;

            for (int y = i; y < max_height; y++) {
                for (int x = j; x < max_width; x++) {
                    int gx = x_start + x;
                    int gy = y_start + y;
                    int dx, dy;

                    if (rotation == ESP_DISPLAY_PRESENT_ROTATE_90) {
                        dx = phys_w - 1 - gy;
                        dy = gx;
                    } else {
                        dx = gy;
                        dy = ver_res - 1 - gx;
                    }

                    const uint8_t *src_pixel = (const uint8_t *)src + (size_t)(y * src_stride_px + x) * color_bytes;
                    uint8_t *dst_pixel = (uint8_t *)dst_fb + (size_t)(dy * phys_w + dx) * color_bytes;

                    if (color_bytes == 2) {
                        *(uint16_t *)dst_pixel = *(const uint16_t *)src_pixel;
                    } else if (color_bytes == 3) {
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                    }
                }
            }
        }
    }
}
