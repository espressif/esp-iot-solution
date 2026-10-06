/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Target region blit: shared tile → framebuffer placement for TE compose and
 * FB partition (same hop; destination topology differs by caller).
 */

#include "present_target_internal.h"

#include "esp_display_present_blit.h"
#include "esp_display_present_target.h"

esp_err_t present_target_blit_region(
    esp_display_present_target_t *target,
    const esp_display_present_blit_fb_t *dst,
    const esp_display_presenter_region_t *region)
{
    if (target == NULL || dst == NULL || region == NULL ||
            region->surface.pixels == NULL ||
            region->surface.width == 0 || region->surface.height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_blit_placement_t placement = {
        .logical = {
            .x1 = region->origin_x,
            .y1 = region->origin_y,
            .x2 = (int32_t)region->origin_x + region->surface.width - 1,
            .y2 = (int32_t)region->origin_y + region->surface.height - 1,
        },
    };
    esp_err_t ret = esp_display_present_target_map_logical_area_to_physical(
                        target, &placement.logical, &placement.physical);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_display_present_blit_tile_to_fb(
               dst, &placement, region->surface.pixels,
               region->surface.stride_bytes);
}
