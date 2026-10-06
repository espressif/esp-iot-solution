/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared FB blit policy: DMA2D / PPA when available, software + cache sync
 * otherwise. Producers call this instead of owning accelerate fallbacks.
 */

#include "esp_display_present_blit.h"

#include <string.h>

#include "esp_display_present_cache.h"
#include "esp_display_present_geometry.h"
#include "esp_display_present_hw.h"
#include "esp_display_present_ppa.h"
#include "esp_display_present_rotate.h"
#include "present_rgb565_swap.h"
#include "soc/soc_caps.h"

/* IDF exposes 2D-DMA framebuffer copies under two names depending on
 * version; both must be honoured or the whole hardware path silently
 * compiles out and every copy falls back to memcpy. */
#if CONFIG_SOC_DMA2D_SUPPORTED &&                                              \
    (defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE) ||                             \
     defined(ESP_ASYNC_FBCPY_AVAILABLE))
#define PRESENT_BLIT_DMA2D_AVAILABLE 1
#else
#define PRESENT_BLIT_DMA2D_AVAILABLE 0
#endif

#if PRESENT_BLIT_DMA2D_AVAILABLE || CONFIG_SOC_PPA_SUPPORTED
#include "esp_cache.h"
#include "esp_memory_utils.h"
#endif

#if PRESENT_BLIT_DMA2D_AVAILABLE
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
#include "esp_async_color_convert.h"

static esp_color_fourcc_t blit_dma2d_pixel_format(uint8_t color_bytes)
{
    if (color_bytes == 2) {
        return ESP_COLOR_FOURCC_RGB16;
    }
    if (color_bytes == 3) {
        return ESP_COLOR_FOURCC_BGR24;
    }
    return 0;
}
#else
#include "esp_async_fbcpy.h"
#if defined(ESP_COLOR_FOURCC_RGB16)
#define PRESENT_BLIT_DMA2D_PIXEL_FORMAT_RGB565                                 \
  .pixel_format_fourcc_id = ESP_COLOR_FOURCC_RGB16
#else
#include "hal/color_types.h"
#define PRESENT_BLIT_DMA2D_PIXEL_FORMAT_RGB565                                 \
  .pixel_format_unique_id = {                                                  \
      .color_type_id = COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565),     \
  }
#endif
#endif
#endif

#if PRESENT_BLIT_DMA2D_AVAILABLE || CONFIG_SOC_PPA_SUPPORTED
static bool cache_writeback_area_rows(const void *base, size_t stride_bytes,
                                      uint16_t y, uint16_t height)
{
    size_t alignment = esp_display_present_get_cache_line_size_by_addr(base);
    if (alignment == 0) {
        return true;
    }
    uintptr_t start = (uintptr_t)base + (size_t)y * stride_bytes;
    uintptr_t end = (uintptr_t)base + (size_t)(y + height) * stride_bytes;
    start &= ~(uintptr_t)(alignment - 1U);
    end = (end + alignment - 1U) & ~(uintptr_t)(alignment - 1U);
    return esp_cache_msync((void *)start, end - start,
                           ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                           ESP_CACHE_MSYNC_FLAG_UNALIGNED) == ESP_OK;
}

#if PRESENT_BLIT_DMA2D_AVAILABLE
static bool cache_invalidate_area_rows(const void *base, size_t stride_bytes,
                                       uint16_t y, uint16_t height)
{
    size_t alignment = esp_display_present_get_cache_line_size_by_addr(base);
    if (alignment == 0) {
        return true;
    }
    uintptr_t start = (uintptr_t)base + (size_t)y * stride_bytes;
    uintptr_t end = (uintptr_t)base + (size_t)(y + height) * stride_bytes;
    start &= ~(uintptr_t)(alignment - 1U);
    end = (end + alignment - 1U) & ~(uintptr_t)(alignment - 1U);
    return esp_cache_msync((void *)start, end - start,
                           ESP_CACHE_MSYNC_FLAG_DIR_M2C) == ESP_OK;
}
#endif
#endif

static void copy_axis_sw(void *dst_fb, size_t dst_stride_bytes,
                         uint8_t color_bytes, const void *src,
                         size_t src_stride_bytes, uint16_t dst_x,
                         uint16_t dst_y, uint16_t width, uint16_t height,
                         bool swap_rgb565)
{
    const size_t row_bytes = (size_t)width * color_bytes;
    const uint8_t *src_row = (const uint8_t *)src;
    uint8_t *dst_base = (uint8_t *)dst_fb + (size_t)dst_y * dst_stride_bytes +
                        (size_t)dst_x * color_bytes;
    for (uint16_t row = 0; row < height; ++row) {
        const uint8_t *in = src_row + (size_t)row * src_stride_bytes;
        uint8_t *out = dst_base + (size_t)row * dst_stride_bytes;
        if (swap_rgb565) {
            present_rgb565_swap_copy((uint16_t *)out, (const uint16_t *)in,
                                     row_bytes / 2U);
        } else {
            memcpy(out, in, row_bytes);
        }
    }
}

#if PRESENT_BLIT_DMA2D_AVAILABLE
static bool copy_axis_dma2d(void *dst_fb, uint16_t dst_fb_w, uint16_t dst_fb_h,
                            size_t dst_stride_bytes, const void *src,
                            uint16_t src_fb_w, uint16_t src_fb_h,
                            size_t src_stride_bytes, uint8_t color_bytes,
                            uint16_t src_x, uint16_t src_y, uint16_t dst_x,
                            uint16_t dst_y, uint16_t width, uint16_t height,
                            bool require_external_dst)
{
    if (require_external_dst && !esp_ptr_external_ram(dst_fb)) {
        return false;
    }
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
    esp_color_fourcc_t format = blit_dma2d_pixel_format(color_bytes);
    if (format == 0) {
        return false;
    }
#else
    if (color_bytes != 2) {
        return false;
    }
#endif
    if (esp_display_present_hw_resource_peek() == NULL) {
        (void)esp_display_present_hw_resource_acquire(2);
    }
    if (esp_display_present_hw_resource_peek() == NULL) {
        return false;
    }
    if (!cache_writeback_area_rows(src, src_stride_bytes, src_y, height) ||
            !cache_writeback_area_rows(dst_fb, dst_stride_bytes, dst_y, height)) {
        return false;
    }
#if defined(ESP_ASYNC_COLOR_CONVERT_AVAILABLE)
    async_color_convert_request_t blit = {
        .src_buffer = src,
        .src_stride = src_fb_w,
        .src_height = src_fb_h,
        .src_x = src_x,
        .src_y = src_y,
        .dst_buffer = dst_fb,
        .dst_stride = dst_fb_w,
        .dst_height = dst_fb_h,
        .dst_x = dst_x,
        .dst_y = dst_y,
        .copy_width = width,
        .copy_height = height,
        .src_color_format = format,
        .dst_color_format = format,
    };
#else
    esp_async_fbcpy_trans_desc_t blit = {
        .src_buffer = (void *)src,
        .dst_buffer = dst_fb,
        .src_buffer_size_x = src_fb_w,
        .src_buffer_size_y = src_fb_h,
        .dst_buffer_size_x = dst_fb_w,
        .dst_buffer_size_y = dst_fb_h,
        .src_offset_x = src_x,
        .src_offset_y = src_y,
        .dst_offset_x = dst_x,
        .dst_offset_y = dst_y,
        .copy_size_x = width,
        .copy_size_y = height,
        PRESENT_BLIT_DMA2D_PIXEL_FORMAT_RGB565,
    };
#endif
    if (esp_display_present_dma2d_copy_sync(&blit, 100) != ESP_OK) {
        return false;
    }
    return cache_invalidate_area_rows(dst_fb, dst_stride_bytes, dst_y, height);
}
#endif

esp_err_t esp_display_present_blit_copy_area(
    const esp_display_present_blit_copy_request_t *request)
{
    if (request == NULL || request->source.pixels == NULL ||
            request->destination.pixels == NULL ||
            request->source.color_bytes == 0 ||
            request->source.color_bytes != request->destination.color_bytes ||
            request->source_area.x1 < 0 || request->source_area.y1 < 0 ||
            request->source_area.x2 < request->source_area.x1 ||
            request->source_area.y2 < request->source_area.y1 ||
            request->destination_origin.x < 0 || request->destination_origin.y < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_display_present_blit_plane_t *src = &request->source;
    const esp_display_present_blit_plane_t *dst = &request->destination;
    int32_t width = request->source_area.x2 - request->source_area.x1 + 1;
    int32_t height = request->source_area.y2 - request->source_area.y1 + 1;
    int32_t src_x = request->source_area.x1;
    int32_t src_y = request->source_area.y1;
    int32_t dst_x = request->destination_origin.x;
    int32_t dst_y = request->destination_origin.y;
    if (width <= 0 || height <= 0 || width > UINT16_MAX ||
            height > UINT16_MAX || request->source_area.x2 >= src->width ||
            request->source_area.y2 >= src->height ||
            dst_x + width > dst->width || dst_y + height > dst->height ||
            src->stride_bytes < (size_t)src->width * src->color_bytes ||
            dst->stride_bytes < (size_t)dst->width * dst->color_bytes ||
            (size_t)(src_x + width) * src->color_bytes > src->stride_bytes ||
            (size_t)(dst_x + width) * dst->color_bytes > dst->stride_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t copy_width = (uint16_t)width;
    uint16_t copy_height = (uint16_t)height;
    uint16_t source_x = (uint16_t)src_x;
    uint16_t source_y = (uint16_t)src_y;
    uint16_t destination_x = (uint16_t)dst_x;
    uint16_t destination_y = (uint16_t)dst_y;
    uint32_t flags = request->flags;
    const bool swap_rgb565 =
        (flags & ESP_DISPLAY_PRESENT_BLIT_COPY_SWAP_RGB565) != 0;
    if (swap_rgb565 && dst->color_bytes != 2) {
        return ESP_ERR_INVALID_ARG;
    }

#if PRESENT_BLIT_DMA2D_AVAILABLE
    if (!swap_rgb565 && (flags & ESP_DISPLAY_PRESENT_BLIT_COPY_TRY_DMA2D) != 0) {
        bool require_ext =
            (flags & ESP_DISPLAY_PRESENT_BLIT_COPY_DMA2D_EXT_DST) != 0;
        if (copy_axis_dma2d(dst->pixels, dst->width, dst->height, dst->stride_bytes,
                            src->pixels, src->width, src->height, src->stride_bytes,
                            dst->color_bytes, source_x, source_y, destination_x, destination_y,
                            copy_width, copy_height, require_ext)) {
            return ESP_OK;
        }
    }
#endif

    const uint8_t *src_base = (const uint8_t *)src->pixels +
                              (size_t)source_y * src->stride_bytes +
                              (size_t)source_x * src->color_bytes;
    copy_axis_sw(dst->pixels, dst->stride_bytes, dst->color_bytes, src_base,
                 src->stride_bytes, destination_x, destination_y, copy_width, copy_height,
                 swap_rgb565);

    if ((flags & ESP_DISPLAY_PRESENT_BLIT_COPY_MSYNC_DST) != 0) {
        esp_display_present_cache_msync_framebuffer(
            (uint8_t *)dst->pixels + (size_t)destination_y * dst->stride_bytes,
            (size_t)copy_height * dst->stride_bytes);
    }
    return ESP_OK;
}

#if CONFIG_SOC_PPA_SUPPORTED
static bool rotate_tile_ppa(const esp_display_present_blit_fb_t *dst,
                            const void *src, size_t src_stride_bytes,
                            const esp_display_present_area_t *physical,
                            uint16_t width, uint16_t height)
{
    if (dst->ppa_handle == NULL || src_stride_bytes % dst->color_bytes != 0) {
        return false;
    }
    uint16_t src_stride_px = (uint16_t)(src_stride_bytes / dst->color_bytes);
    /*
     * Only write back the SRAM tile. Do NOT row-sync the destination here:
     * for 90/270 a logical horizontal band maps to a full-height vertical
     * strip, so y-span row msync would rewrite the entire FB every band.
     * Caller (repair/switch submit) owns one coherent FB cache sync.
     */
    if (src_stride_px < width ||
            !cache_writeback_area_rows(src, src_stride_bytes, 0, height)) {
        return false;
    }
    esp_display_present_ppa_rotate_block_request_t request = {
        .source = {
            .pixels = (void *)src,
            .width = src_stride_px,
            .height = height,
            .stride_bytes = src_stride_bytes,
            .color_bytes = dst->color_bytes,
        },
        .source_area = {
            .x1 = 0,
            .y1 = 0,
            .x2 = width - 1,
            .y2 = height - 1,
        },
        .destination = {
            .pixels = dst->pixels,
            .width = dst->width,
            .height = dst->height,
            .stride_bytes = dst->stride_bytes,
            .color_bytes = dst->color_bytes,
        },
        .destination_origin = {
            .x = physical->x1,
            .y = physical->y1,
        },
        .rotation = dst->rotation,
    };
    return esp_display_present_ppa_rotate_copy_block(dst->ppa_handle, &request) ==
           ESP_OK;
}
#endif

static esp_err_t
blit_tile_rotated(const esp_display_present_blit_fb_t *dst,
                  const esp_display_present_blit_placement_t *placement,
                  const void *src, size_t src_stride_bytes)
{
    uint16_t height =
        (uint16_t)(placement->logical.y2 - placement->logical.y1 + 1);

#if CONFIG_SOC_PPA_SUPPORTED
    uint16_t width =
        (uint16_t)(placement->logical.x2 - placement->logical.x1 + 1);
    if (rotate_tile_ppa(dst, src, src_stride_bytes, &placement->physical, width,
                        height)) {
        return ESP_OK;
    }
#endif

    uint16_t src_stride_px = (uint16_t)(src_stride_bytes / dst->color_bytes);
    esp_display_present_rotate_copy_request_t request = {
        .source = {
            .pixels = (void *)src,
            .width = src_stride_px,
            .height = height,
            .stride_bytes = src_stride_bytes,
            .color_bytes = dst->color_bytes,
        },
        .source_origin = {
            .x = placement->logical.x1,
            .y = placement->logical.y1,
        },
        .destination = {
            .pixels = dst->pixels,
            .width = dst->width,
            .height = dst->height,
            .stride_bytes = dst->stride_bytes,
            .color_bytes = dst->color_bytes,
        },
        .logical_area = placement->logical,
        .rotation = dst->rotation,
    };
    esp_display_present_rotate_copy(&request);
    /*
     * Same 90/270 trap as PPA: defer dest cache maintenance to frame
     * commit so a vertical strip does not force full-FB row syncs per band.
     */
    return ESP_OK;
}

esp_err_t esp_display_present_blit_tile_to_fb(
    const esp_display_present_blit_fb_t *dst,
    const esp_display_present_blit_placement_t *placement, const void *src,
    size_t src_stride_bytes)
{
    if (dst == NULL || dst->pixels == NULL || placement == NULL || src == NULL ||
            dst->color_bytes == 0 || placement->logical.x1 < 0 ||
            placement->logical.y1 < 0 ||
            placement->logical.x2 < placement->logical.x1 ||
            placement->logical.y2 < placement->logical.y1 ||
            placement->physical.x1 < 0 || placement->physical.y1 < 0 ||
            placement->physical.x2 < placement->physical.x1 ||
            placement->physical.y2 < placement->physical.y1 ||
            placement->physical.x2 >= dst->width ||
            placement->physical.y2 >= dst->height) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t width =
        (uint16_t)(placement->logical.x2 - placement->logical.x1 + 1);
    uint16_t height =
        (uint16_t)(placement->logical.y2 - placement->logical.y1 + 1);
    uint16_t physical_width =
        (uint16_t)(placement->physical.x2 - placement->physical.x1 + 1);
    uint16_t physical_height =
        (uint16_t)(placement->physical.y2 - placement->physical.y1 + 1);
    bool quarter_turn = dst->rotation == ESP_DISPLAY_PRESENT_ROTATE_90 ||
                        dst->rotation == ESP_DISPLAY_PRESENT_ROTATE_270;
    if (src_stride_bytes < (size_t)width * dst->color_bytes ||
            physical_width != (quarter_turn ? height : width) ||
            physical_height != (quarter_turn ? width : height)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t logical_width = quarter_turn ? dst->height : dst->width;
    uint16_t logical_height = quarter_turn ? dst->width : dst->height;
    esp_display_present_area_t expected_physical;
    if (esp_display_present_geometry_map_logical_area_to_physical(dst->rotation, (esp_display_present_size_t) {
    .width = logical_width,
    .height = logical_height,
}, &placement->logical,
&expected_physical) != ESP_OK ||
memcmp(&expected_physical, &placement->physical,
       sizeof(expected_physical)) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dst->rotation != ESP_DISPLAY_PRESENT_ROTATE_0) {
        return blit_tile_rotated(dst, placement, src, src_stride_bytes);
    }

    esp_display_present_blit_plane_t dst_plane = {
        .pixels = dst->pixels,
        .width = dst->width,
        .height = dst->height,
        .stride_bytes = dst->stride_bytes,
        .color_bytes = dst->color_bytes,
    };
    esp_display_present_blit_plane_t src_plane = {
        .pixels = (void *)src,
        .width = width,
        .height = height,
        .stride_bytes = src_stride_bytes,
        .color_bytes = dst->color_bytes,
    };
    esp_display_present_blit_copy_request_t request = {
        .source = src_plane,
        .source_area = {
            .x1 = 0,
            .y1 = 0,
            .x2 = width - 1,
            .y2 = height - 1,
        },
        .destination = dst_plane,
        .destination_origin = {
            .x = placement->physical.x1,
            .y = placement->physical.y1,
        },
        .flags = ESP_DISPLAY_PRESENT_BLIT_COPY_TRY_DMA2D |
        ESP_DISPLAY_PRESENT_BLIT_COPY_DMA2D_EXT_DST |
        ESP_DISPLAY_PRESENT_BLIT_COPY_MSYNC_DST,
    };
    return esp_display_present_blit_copy_area(&request);
}

esp_err_t esp_display_present_blit_copy_framebuffer_area(
    const esp_display_present_blit_plane_t *dst,
    const esp_display_present_blit_plane_t *src,
    const esp_display_present_area_t *physical_area)
{
    if (dst == NULL || src == NULL || physical_area == NULL ||
            dst->pixels == NULL || src->pixels == NULL || dst->width != src->width ||
            dst->height != src->height || dst->stride_bytes != src->stride_bytes ||
            dst->color_bytes != src->color_bytes || dst->color_bytes == 0 ||
            physical_area->x1 < 0 || physical_area->y1 < 0 ||
            physical_area->x2 < physical_area->x1 ||
            physical_area->y2 < physical_area->y1 ||
            physical_area->x2 >= dst->width || physical_area->y2 >= dst->height) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t x = (uint16_t)physical_area->x1;
    uint16_t y = (uint16_t)physical_area->y1;
    esp_display_present_blit_copy_request_t request = {
        .source = *src,
        .source_area = *physical_area,
        .destination = *dst,
        .destination_origin = {
            .x = x,
            .y = y,
        },
        .flags = ESP_DISPLAY_PRESENT_BLIT_COPY_TRY_DMA2D,
    };
    return esp_display_present_blit_copy_area(&request);
}
