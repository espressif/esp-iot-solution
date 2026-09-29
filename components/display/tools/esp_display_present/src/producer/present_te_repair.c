/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_te_internal.h"
#include "present_mode_internal.h"

esp_err_t present_te_compose_repair_create(
    esp_display_present_te_compose_t *te)
{
    if (te == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return present_buffer_repair_create(
               &te->repair, te->compose_buffer_count, te->max_damage_areas,
               true, true);
}

void present_te_compose_repair_destroy(
    esp_display_present_te_compose_t *te)
{
    if (te != NULL) {
        present_buffer_repair_destroy(&te->repair);
    }
}

esp_err_t esp_display_present_te_compose_repair(
    present_frame_ctx_t *ctx, bool *out_repaired)
{
    esp_display_present_te_compose_t *te =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (out_repaired != NULL) {
        *out_repaired = false;
    }
    if (te != NULL) {
        esp_err_t copy_ret = present_async_copy_drain(&te->async_copy);
        if (copy_ret != ESP_OK) {
            return copy_ret;
        }
    }
    if (te == NULL || ctx == NULL || !te->repair.enabled ||
            !te->has_active_buffer || te->display_buffer == NULL) {
        return te != NULL ? ESP_OK : ESP_ERR_INVALID_ARG;
    }
    present_buffer_repair_view_t view = {
        .target = te->target,
        .buffer_count = te->compose_buffer_count,
        .display_buffer = te->display_buffer,
        .draw_buffer = te->draw.pixels,
        .logical_width = te->logical_width,
        .logical_height = te->logical_height,
        .physical_width = te->draw.width,
        .physical_height = te->draw.height,
        .physical_stride_bytes = te->draw.stride_bytes,
        .color_bytes = te->draw.color_bytes,
    };
    for (uint8_t index = 0; index < view.buffer_count; ++index) {
        view.buffers[index] = te->draw.buffers[index];
    }
    esp_err_t ret = present_buffer_repair_apply(
                        &te->repair, &view, ctx->repair.rendered_areas,
                        ctx->repair.rendered_area_count);
    if (ret == ESP_OK && out_repaired != NULL) {
        *out_repaired = true;
    }
    return ret;
}
