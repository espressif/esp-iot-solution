/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Partition tile path: ACQUIRE tile + SUBMIT blit→draw_fb (two hops, one
 * topology). Shared helpers live in present_mode_internal.h / present_tile_acquire.c.
 */

#include "present_fb_internal.h"

#include "esp_display_present_blit.h"
#include "present_mode_internal.h"
#include "present_target_internal.h"

static bool partition_area_valid(
    const esp_display_present_fb_endpoint_t *surface,
    const esp_display_presenter_region_t *region)
{
    return surface != NULL && region != NULL &&
           region->surface.width != 0 && region->surface.height != 0 &&
           (uint32_t)region->origin_x + region->surface.width <=
           surface->width &&
           (uint32_t)region->origin_y + region->surface.height <=
           surface->height;
}

esp_err_t esp_display_present_fb_acquire_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region)
{
    esp_display_present_fb_endpoint_t *surface =
        ctx != NULL ? ctx->mode_ctx : NULL;
    const uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    if (surface == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = present_fb_ensure_lease(surface, frame_id);
    if (ret != ESP_OK) {
        return ret;
    }
    present_async_copy_wait_slot(&surface->fb.async_copy,
                                 surface->fb.next_drawbuf);
    return present_stage_acquire_tile(
               &surface->fb.drawbuf_pool, &surface->fb.next_drawbuf,
               surface->color_bytes, area, pixel_format, out_region);
}

esp_err_t esp_display_present_fb_write_partition(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region)
{
    esp_display_present_fb_endpoint_t *surface =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (surface == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_display_present_fb_lease_t *lease = &surface->lease.valet;
    if (!present_fb_lease_is_valid(surface, lease) ||
            region == NULL || region->surface.pixels == NULL ||
            !partition_area_valid(surface, region) ||
            region->surface.stride_bytes <
            (size_t)region->surface.width * surface->color_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    void *destination = lease->surface.pixels;
    if (destination == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_display_present_blit_fb_t dst = {
        .pixels = destination,
        .width = surface->physical_width,
        .height = surface->physical_height,
        .stride_bytes = surface->physical_stride_bytes,
        .color_bytes = surface->color_bytes,
        .rotation = surface->rotation,
        .ppa_handle = surface->fb.ppa_handle,
    };
    if (present_async_copy_submit(&surface->fb.async_copy, surface->target,
                                  &dst, region, false) == ESP_OK) {
        return ESP_OK;
    }
    /* Row-span cache invalidation in a queued blit would drop an inline
     * CPU copy into the same rows, so never run both at once. */
    esp_err_t ret = present_async_copy_drain(&surface->fb.async_copy);
    if (ret != ESP_OK) {
        return ret;
    }
    return present_target_blit_region(surface->target, &dst, region);
}
