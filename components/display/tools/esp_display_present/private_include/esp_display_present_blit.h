/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_display_present_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Shared axis-aligned region copy policy for producers.
 *
 * All partition compose / extract / repair complement paths should go through
 * this header instead of owning private memcpy / DMA2D loops.
 */

/** Linear image plane used by axis-aligned copies. */
typedef struct {
    void *pixels;
    uint16_t width;
    uint16_t height;
    size_t stride_bytes;
    uint8_t color_bytes;
} esp_display_present_blit_plane_t;

typedef enum {
    ESP_DISPLAY_PRESENT_BLIT_COPY_NONE = 0,
    /** RGB565 only: byte-swap each pixel while copying (forces SW path). */
    ESP_DISPLAY_PRESENT_BLIT_COPY_SWAP_RGB565 = (1u << 0),
    /** Try DMA2D when geometry/format allow; always has SW fallback. */
    ESP_DISPLAY_PRESENT_BLIT_COPY_TRY_DMA2D = (1u << 1),
    /**
     * After a software copy, msync the destination row span.
     * DMA2D success already maintains cache coherency.
     */
    ESP_DISPLAY_PRESENT_BLIT_COPY_MSYNC_DST = (1u << 2),
    /**
     * DMA2D only when destination is external RAM (PSRAM FB). Used by
     * partition tile→FB compose.
     */
    ESP_DISPLAY_PRESENT_BLIT_COPY_DMA2D_EXT_DST = (1u << 3),
} esp_display_present_blit_copy_flags_t;

/**
 * One axis-aligned source area copied to a destination origin.
 *
 * Coordinates are in their respective plane's pixel spaces. Bounds are
 * checked against both planes.
 */
typedef struct {
    esp_display_present_blit_plane_t source;
    esp_display_present_area_t source_area;
    esp_display_present_blit_plane_t destination;
    esp_display_present_point_t destination_origin;
    uint32_t flags;
} esp_display_present_blit_copy_request_t;

esp_err_t esp_display_present_blit_copy_area(
    const esp_display_present_blit_copy_request_t *request);

/**
 * Physical framebuffer destination for tile / region blits.
 *
 * Owns the accelerate-or-fallback policy for writing into a scanout /
 * draw framebuffer (DMA2D, PPA, software + cache sync).
 */
typedef struct {
    void *pixels;
    uint16_t width;
    uint16_t height;
    size_t stride_bytes;
    uint8_t color_bytes;
    esp_display_present_rotation_t rotation;
    /** Optional PPA SRM client; NULL forces software rotate. */
    void *ppa_handle;
} esp_display_present_blit_fb_t;

/** Geometry resolved by the producer before entering the pixel layer. */
typedef struct {
    esp_display_present_area_t logical;
    esp_display_present_area_t physical;
} esp_display_present_blit_placement_t;

/**
 * Copy a tile into @p dst, applying panel rotation when needed.
 *
 * @p src is typically a tight-packed partition drawbuf (stride == width).
 * The producer supplies matching logical and physical areas so this pixel
 * primitive does not depend on target ownership or panel policy.
 */
esp_err_t esp_display_present_blit_tile_to_fb(
    const esp_display_present_blit_fb_t *dst,
    const esp_display_present_blit_placement_t *placement, const void *src,
    size_t src_stride_bytes);

/**
 * Copy one physical rectangle between equal-geometry framebuffers.
 *
 * Used by repair complement fill (disp_fb → draw_fb).
 */
esp_err_t esp_display_present_blit_copy_framebuffer_area(
    const esp_display_present_blit_plane_t *dst,
    const esp_display_present_blit_plane_t *src,
    const esp_display_present_area_t *physical_area);

#ifdef __cplusplus
}
#endif
