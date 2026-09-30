/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_display_present_config.h"

/** Where submitted pixels live before they reach the panel. */
typedef enum {
    ESP_DISPLAY_PRESENT_STORAGE_GRAM = 0,
    ESP_DISPLAY_PRESENT_STORAGE_SINGLE_FB,
    ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB,
} esp_display_present_storage_t;

/** How a surface producer keeps the submitted image coherent. */
typedef enum {
    ESP_DISPLAY_PRESENT_FB_PERSISTENT = 0,
    ESP_DISPLAY_PRESENT_FB_FULL,
    ESP_DISPLAY_PRESENT_FB_DIRECT,
    ESP_DISPLAY_PRESENT_FB_REPAIR,
} esp_display_present_fb_contract_t;

/** External pacing source, if any. */
typedef enum {
    ESP_DISPLAY_PRESENT_SYNC_NONE = 0,
    ESP_DISPLAY_PRESENT_SYNC_TE,
} esp_display_present_sync_t;

/** What a frame-done interrupt releases. */
typedef enum {
    ESP_DISPLAY_PRESENT_FRAME_DONE_RELEASE_NONE = 0,
    ESP_DISPLAY_PRESENT_FRAME_DONE_RELEASE_SUBMIT,
} esp_display_present_frame_done_release_t;

/** Immutable present profile resolved once while creating a target. */
typedef struct {
    esp_display_present_mode_t mode;
    esp_display_present_storage_t storage;
    esp_display_present_fb_contract_t fb;
    esp_display_present_sync_t sync;
    esp_display_present_frame_done_release_t frame_done_release;
    uint8_t frame_buffer_count;
} esp_display_present_profile_t;

/**
 * Resolve and validate the static presentation policy without accessing
 * hardware. Runtime AUTO panel detection uses only IO handle presence.
 */
esp_err_t esp_display_present_profile_resolve(
    const esp_display_present_target_config_t *config,
    esp_display_present_panel_interface_t *out_panel_interface,
    esp_display_present_profile_t *out_profile);
