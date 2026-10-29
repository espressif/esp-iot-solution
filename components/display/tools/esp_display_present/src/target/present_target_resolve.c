/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_target_internal.h"

#include <stddef.h>
#include <string.h>

#include "esp_display_present_panel.h"
#include "esp_display_present_pipeline_policy.h"
#include "esp_display_present_profile.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "present_target_resolve";

#define DEFAULT_STAGING_BYTES (32U * 1024U)
#define DEFAULT_STAGING_BUFFERS 2U
/*
 * Internal DMA heap a default second drawbuf must leave free once both
 * drawbufs are allocated, for the renderer task and pools created after the
 * target.
 */
#define DOUBLE_DRAWBUF_HEADROOM_BYTES (128U * 1024U)

static uint8_t pixel_format_bytes(
    esp_display_present_pixel_format_t pixel_format)
{
    switch (pixel_format) {
    case ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565:
        return 2;
    case ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888:
        return 3;
    default:
        return 0;
    }
}

static uint16_t resolve_staging_lines(uint16_t width, uint16_t height,
                                      uint8_t color_bytes,
                                      uint16_t configured_lines)
{
    if (configured_lines != 0) {
        return configured_lines;
    }
    size_t row_bytes = (size_t)width * color_bytes;
    size_t lines = DEFAULT_STAGING_BYTES / row_bytes;
    if (lines == 0) {
        lines = 1;
    }
    return (uint16_t)(lines < height ? lines : height);
}

static bool internal_heap_spares(size_t reserved_bytes)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) >=
           reserved_bytes;
}

static bool fb_repair_second_drawbuf_fits(
    const esp_display_present_drawbuf_config_t *config, size_t drawbuf_bytes)
{
    if (portNUM_PROCESSORS < 2) {
        return false;
    }
    return config->in_psram ||
           internal_heap_spares(DEFAULT_STAGING_BUFFERS * drawbuf_bytes +
                                DOUBLE_DRAWBUF_HEADROOM_BYTES);
}

static bool te_second_drawbuf_fits(
    const esp_display_present_drawbuf_config_t *config, uint16_t width,
    uint16_t height, uint8_t color_bytes, size_t drawbuf_bytes)
{
    if (portNUM_PROCESSORS < 2) {
        return false;
    }
    if (config->in_psram) {
        return true;
    }
    size_t reserved = DEFAULT_STAGING_BUFFERS * drawbuf_bytes +
                      DOUBLE_DRAWBUF_HEADROOM_BYTES;
    /* Compose buffers fall back to internal RAM after the drawbufs. Query the
     * same capabilities used by te_render.c, not SPIRAM alone. */
    size_t compose_bytes = (size_t)width * height * color_bytes;
    const uint32_t compose_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA |
                                  MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED;
    if (heap_caps_get_largest_free_block(compose_caps) < compose_bytes) {
        reserved += compose_bytes *
                    (config->te_compose_buffers >= 2 ? 2U : 1U);
    }
    return internal_heap_spares(reserved);
}

static esp_display_present_drawbuf_info_t resolve_drawbuf_info(
    const esp_display_present_profile_t *profile,
    uint16_t width,
    uint16_t height,
    uint8_t color_bytes,
    const esp_display_present_drawbuf_config_t *config)
{
    if (profile == NULL || config == NULL ||
            (profile->storage != ESP_DISPLAY_PRESENT_STORAGE_GRAM &&
             profile->fb != ESP_DISPLAY_PRESENT_FB_REPAIR)) {
        return (esp_display_present_drawbuf_info_t) {
            0
        };
    }
    uint16_t lines = resolve_staging_lines(
                         width, height, color_bytes, config->lines);
    size_t drawbuf_bytes = (size_t)lines * width * color_bytes;
    /* A second bounded drawbuf lets the async worker copy one band while the
     * next renders; the worker needs a second core to overlap. */
    bool second_fits;
    bool optional_second = false;
    if (profile->fb == ESP_DISPLAY_PRESENT_FB_REPAIR) {
        second_fits = fb_repair_second_drawbuf_fits(config, drawbuf_bytes);
        optional_second = second_fits;
    } else if (profile->sync == ESP_DISPLAY_PRESENT_SYNC_TE) {
        second_fits = te_second_drawbuf_fits(config, width, height,
                                             color_bytes, drawbuf_bytes);
        optional_second = second_fits;
    } else {
        second_fits = true;
    }
    uint8_t default_buffers = second_fits ? DEFAULT_STAGING_BUFFERS : 1;
    return (esp_display_present_drawbuf_info_t) {
        .lines = lines,
        .buffers = config->buffers != 0
                   ? config->buffers : default_buffers,
                   .in_psram = config->in_psram,
                   .optional_second = config->buffers == 0 && optional_second,
    };
}

static esp_display_present_pipeline_policy_t resolve_pipeline_policy(
    const esp_display_present_profile_t *profile,
    uint16_t height,
    const esp_display_present_drawbuf_info_t *drawbuf)
{
    esp_display_present_pipeline_policy_t policy = {
        .commit = ESP_DISPLAY_PRESENT_FRAME_COMMIT_SWITCH,
        .slot_count = profile != NULL ? profile->frame_buffer_count : 0,
        .slot_lines = height,
    };
    if (profile == NULL) {
        return policy;
    }

    if (profile->storage == ESP_DISPLAY_PRESENT_STORAGE_GRAM) {
        if (profile->sync == ESP_DISPLAY_PRESENT_SYNC_TE) {
            policy.commit = ESP_DISPLAY_PRESENT_FRAME_COMMIT_TE_PUSH;
        } else {
            policy.commit = ESP_DISPLAY_PRESENT_FRAME_COMMIT_WAIT_INFLIGHT;
        }
        policy.slot_count = drawbuf != NULL ? drawbuf->buffers : 0;
        policy.slot_lines = drawbuf != NULL ? drawbuf->lines : 0;
        return policy;
    }

    if (profile->fb == ESP_DISPLAY_PRESENT_FB_REPAIR) {
        policy.slot_count = drawbuf != NULL ? drawbuf->buffers : 0;
        policy.slot_lines = drawbuf != NULL ? drawbuf->lines : 0;
    }
    return policy;
}

static esp_err_t borrow_frame_buffers(
    void *const *buffers,
    uint8_t buffer_count,
    uint8_t required,
    esp_display_present_frame_buffers_t *out_fbs)
{
    if (buffers == NULL || out_fbs == NULL || buffer_count != required) {
        return ESP_ERR_INVALID_ARG;
    }
    for (uint8_t index = 0; index < required; ++index) {
        if (buffers[index] == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        out_fbs->buffers[index] = buffers[index];
    }
    out_fbs->count = required;
    return ESP_OK;
}

static esp_err_t acquire_frame_buffers(
    esp_lcd_panel_handle_t panel,
    esp_display_present_panel_interface_t panel_interface,
    void *const *caller_buffers,
    uint8_t caller_buffer_count,
    size_t frame_bytes,
    uint8_t required,
    esp_display_present_frame_buffers_t *out_fbs)
{
    if (out_fbs == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_fbs = (esp_display_present_frame_buffers_t) {
        0
    };
    if (required == 0) {
        return ESP_OK;
    }

    if (caller_buffer_count != 0) {
        esp_err_t ret = borrow_frame_buffers(
                            caller_buffers, caller_buffer_count, required, out_fbs);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG,
                     "fb config count=%u incompatible with mode needing %u",
                     (unsigned)caller_buffer_count, (unsigned)required);
        }
        return ret;
    }

    esp_err_t ret = esp_display_present_fetch_panel_frame_buffers(
                        panel, panel_interface, required, frame_bytes, out_fbs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "panel has fewer framebuffers than mode requires "
                 "(need %u); create the panel with matching num_fbs "
                 "or pick a compatible present mode",
                 (unsigned)required);
    }
    return ret;
}

esp_err_t present_target_resolve_info(
    const esp_display_present_target_config_t *config,
    esp_display_present_pixel_format_t pixel_format,
    uint16_t width,
    uint16_t height,
    esp_display_present_target_info_t *out_info)
{
    if (out_info != NULL) {
        memset(out_info, 0, sizeof(*out_info));
    }
    uint8_t color_bytes = pixel_format_bytes(pixel_format);
    if (config == NULL || config->hw.panel == NULL || width == 0 ||
            height == 0 || out_info == NULL ||
            config->fb.frame_buffer_count > ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS ||
            config->hw.input_pixel_format != pixel_format || color_bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_display_present_panel_interface_t panel_interface;
    esp_display_present_profile_t profile;
    esp_err_t ret = esp_display_present_profile_resolve(
                        config, &panel_interface, &profile);
    if (ret != ESP_OK) {
        return ret;
    }
    bool panel_gram = panel_interface == ESP_DISPLAY_PRESENT_PANEL_IF_OTHER;
    if (panel_gram && config->hw.io == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    bool te_enabled = config->hw.te_enabled &&
                      esp_display_present_te_sync_is_enabled(&config->hw.te_sync);
    if ((profile.storage == ESP_DISPLAY_PRESENT_STORAGE_GRAM ||
            profile.fb == ESP_DISPLAY_PRESENT_FB_REPAIR) &&
            (config->drawbuf.buffers > 2 ||
             config->drawbuf.te_compose_buffers > 2 ||
             config->drawbuf.lines > height)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_display_present_drawbuf_info_t drawbuf = resolve_drawbuf_info(
                                                     &profile, width, height, color_bytes, &config->drawbuf);
    esp_display_present_pipeline_policy_t pipeline = resolve_pipeline_policy(
                                                         &profile, height, &drawbuf);

    esp_display_present_frame_buffers_t frame_buffers = {0};
    ret = acquire_frame_buffers(
              config->hw.panel, panel_interface,
              config->fb.frame_buffers, config->fb.frame_buffer_count,
              (size_t)width * height * color_bytes, profile.frame_buffer_count,
              &frame_buffers);
    if (ret != ESP_OK) {
        return ret;
    }

    *out_info = (esp_display_present_target_info_t) {
        .hw = {
            .panel = config->hw.panel,
            .io = config->hw.io,
            .panel_interface = panel_interface,
            .pixel_format = pixel_format,
            .rotation = config->hw.rotation,
            .te_sync = te_enabled ? config->hw.te_sync :
            ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED(),
            .color_bytes = color_bytes,
            .width = width,
            .height = height,
            .panel_gram = panel_gram,
            .swap_bytes = config->hw.swap_bytes,
        },
        .fb = {
            .profile = profile,
            .frame_buffer_count = frame_buffers.count,
        },
        .pipeline = pipeline,
        .drawbuf = drawbuf,
    };
    out_info->drawbuf.te_compose_buffers = 1;
    if (profile.sync == ESP_DISPLAY_PRESENT_SYNC_TE &&
            config->drawbuf.te_compose_buffers >= 2) {
        out_info->drawbuf.te_compose_buffers =
            config->drawbuf.te_compose_buffers > ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS
            ? ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS
            : config->drawbuf.te_compose_buffers;
    }
    for (uint8_t index = 0; index < frame_buffers.count; ++index) {
        out_info->fb.frame_buffers[index] = frame_buffers.buffers[index];
    }
    return ESP_OK;
}
