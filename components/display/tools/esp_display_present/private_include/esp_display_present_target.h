/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_display_present_config.h"
#include "esp_display_present_te.h"
#include "esp_display_present_pipeline_policy.h"
#include "esp_display_present_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Resolved panel / bus fact used by producers and ISR registration. */
typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    esp_display_present_panel_interface_t panel_interface;
    esp_display_present_pixel_format_t pixel_format;
    esp_display_present_rotation_t rotation;
    esp_display_present_te_sync_config_t te_sync;
    uint8_t color_bytes;
    uint16_t width;
    uint16_t height;
    bool panel_gram;
    bool swap_bytes;
} esp_display_present_hw_info_t;

/** Resolved frame-buffer present profile and borrowed panel FBs. */
typedef struct {
    esp_display_present_profile_t profile;
    void *frame_buffers[ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS];
    uint8_t frame_buffer_count;
} esp_display_present_fb_info_t;

/** Resolved shared partition drawbuf; memory is owned by the facade. */
typedef struct {
    uint16_t lines;
    uint8_t buffers;
    uint8_t te_compose_buffers;
    bool in_psram;
    /** The second buffer came from the default policy; drop it if it cannot be allocated. */
    bool optional_second;
} esp_display_present_drawbuf_info_t;

/** Resolved immutable target contract shared by presentation producers. */
typedef struct {
    esp_display_present_hw_info_t hw;
    esp_display_present_fb_info_t fb;
    esp_display_present_pipeline_policy_t pipeline;
    esp_display_present_drawbuf_info_t drawbuf;
} esp_display_present_target_info_t;

typedef struct esp_display_present_target esp_display_present_target_t;

typedef struct {
    bool (*on_transfer_done)(void *user_ctx);
    bool (*on_frame_done)(void *user_ctx);
} esp_display_present_target_callbacks_t;

/** Resolve panel policy and framebuffer ownership into one present target. */
esp_err_t esp_display_present_target_create(
    const esp_display_present_target_config_t *config,
    esp_display_present_pixel_format_t pixel_format,
    uint16_t width,
    uint16_t height,
    esp_display_present_target_t **out_target);

/**
 * Register the target's interface-specific hardware callbacks.
 * Call only after the renderer-side present state is initialized.
 */
esp_err_t esp_display_present_target_set_callbacks(
    esp_display_present_target_t *target,
    const esp_display_present_target_callbacks_t *callbacks,
    void *user_ctx);

/** Unregister hardware callbacks and drain callbacks already in progress. */
esp_err_t esp_display_present_target_clear_callbacks(
    esp_display_present_target_t *target);

const esp_display_present_target_info_t *esp_display_present_target_get_info(
    const esp_display_present_target_t *target);

/** Map a physical panel coordinate into the target's logical orientation. */
esp_err_t esp_display_present_target_map_physical_point_to_logical(
    const esp_display_present_target_t *target,
    int32_t *x,
    int32_t *y);

/** Map an inclusive logical rectangle into physical panel coordinates. */
esp_err_t esp_display_present_target_map_logical_area_to_physical(
    const esp_display_present_target_t *target,
    const esp_display_present_area_t *logical,
    esp_display_present_area_t *out_physical);

bool IRAM_ATTR esp_display_present_target_notify_transfer_done_from_isr(
    esp_display_present_target_t *target);

bool IRAM_ATTR esp_display_present_target_notify_frame_done_from_isr(
    esp_display_present_target_t *target);

/** Delete a target after its hardware callbacks have been cleared. */
esp_err_t esp_display_present_target_delete(
    esp_display_present_target_t *target);

#ifdef __cplusplus
}
#endif
