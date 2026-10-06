/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present.h"
#include "esp_display_present_profile.h"
#include "esp_display_present_te.h"

static bool panel_uses_gram_storage(
    esp_display_present_panel_interface_t panel_interface)
{
    return panel_interface == ESP_DISPLAY_PRESENT_PANEL_IF_OTHER;
}

static esp_display_present_mode_t resolve_mode(
    esp_display_present_mode_t requested_mode,
    esp_display_present_panel_interface_t panel_interface,
    bool te_enabled)
{
    if (requested_mode != ESP_DISPLAY_PRESENT_MODE_AUTO) {
        return requested_mode;
    }
    if (panel_uses_gram_storage(panel_interface)) {
        return te_enabled ? ESP_DISPLAY_PRESENT_MODE_TE_SYNC :
               ESP_DISPLAY_PRESENT_MODE_NONE;
    }
    return ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL;
}

static esp_err_t resolve_profile(
    esp_display_present_mode_t requested_mode,
    esp_display_present_panel_interface_t panel_interface,
    bool te_enabled,
    esp_display_present_profile_t *out_profile)
{
    if (out_profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool uses_gram_storage = panel_uses_gram_storage(panel_interface);
    esp_display_present_mode_t mode = resolve_mode(
                                          requested_mode, panel_interface, te_enabled);
    esp_display_present_profile_t profile = {
        .mode = mode,
        .storage = uses_gram_storage
        ? ESP_DISPLAY_PRESENT_STORAGE_GRAM
        : ESP_DISPLAY_PRESENT_STORAGE_SINGLE_FB,
        .fb = ESP_DISPLAY_PRESENT_FB_PERSISTENT,
        .sync = mode == ESP_DISPLAY_PRESENT_MODE_TE_SYNC
        ? ESP_DISPLAY_PRESENT_SYNC_TE
        : ESP_DISPLAY_PRESENT_SYNC_NONE,
        .frame_buffer_count = 1,
    };

    switch (mode) {
    case ESP_DISPLAY_PRESENT_MODE_NONE:
    case ESP_DISPLAY_PRESENT_MODE_TE_SYNC:
        break;
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL:
        profile.frame_buffer_count = 2;
        profile.storage = ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB;
        profile.fb = ESP_DISPLAY_PRESENT_FB_FULL;
        break;
    case ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL:
        profile.frame_buffer_count = 3;
        profile.storage = ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB;
        profile.fb = ESP_DISPLAY_PRESENT_FB_FULL;
        break;
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT:
        profile.frame_buffer_count = 2;
        profile.storage = ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB;
        profile.fb = ESP_DISPLAY_PRESENT_FB_DIRECT;
        break;
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL:
        profile.frame_buffer_count = 2;
        profile.storage = ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB;
        profile.fb = ESP_DISPLAY_PRESENT_FB_REPAIR;
        break;
    case ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL:
        profile.frame_buffer_count = 3;
        profile.storage = ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB;
        profile.fb = ESP_DISPLAY_PRESENT_FB_REPAIR;
        break;
    case ESP_DISPLAY_PRESENT_MODE_AUTO:
    default:
        return ESP_ERR_INVALID_ARG;
    }

    if (uses_gram_storage) {
        /* GRAM path: storage/surface from the GRAM defaults above; modes
         * other than NONE/TE_SYNC are rejected by validate(). */
        profile.storage = ESP_DISPLAY_PRESENT_STORAGE_GRAM;
        profile.fb = ESP_DISPLAY_PRESENT_FB_PERSISTENT;
        profile.frame_buffer_count = 0;
        profile.frame_done_release = ESP_DISPLAY_PRESENT_FRAME_DONE_RELEASE_NONE;
    } else {
        profile.frame_done_release =
            profile.fb == ESP_DISPLAY_PRESENT_FB_REPAIR ||
            panel_interface == ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI
            ? ESP_DISPLAY_PRESENT_FRAME_DONE_RELEASE_SUBMIT
            : ESP_DISPLAY_PRESENT_FRAME_DONE_RELEASE_NONE;
    }

    *out_profile = profile;
    return ESP_OK;
}

static esp_err_t validate_profile(
    const esp_display_present_profile_t *profile,
    esp_display_present_panel_interface_t panel_interface,
    bool te_enabled,
    esp_display_present_rotation_t rotation)
{
    if (profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool uses_gram_storage = panel_uses_gram_storage(panel_interface);
    bool te = profile->sync == ESP_DISPLAY_PRESENT_SYNC_TE;

    if (uses_gram_storage) {
        /*
         * Plain GRAM direct output cannot rotate because SUBMIT immediately
         * kicks the rendered tile to the panel.  TE composes tiles into a
         * full draw framebuffer first, so rotation can be fused into that
         * tile->FB placement.
         */
        if (!te && rotation != ESP_DISPLAY_PRESENT_ROTATE_0) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        if (profile->mode != ESP_DISPLAY_PRESENT_MODE_NONE && !te) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        if (te && !te_enabled) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        return ESP_OK;
    }

    /* RGB / MIPI: TE sync profiles are GRAM-only. */
    if (te) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* DIRECT means rendering into a coherent scanout-oriented framebuffer.
     * A rotated logical transform surface is a FULL-style fallback and must
     * not be exposed under the DIRECT contract. */
    if (profile->fb == ESP_DISPLAY_PRESENT_FB_DIRECT &&
            rotation != ESP_DISPLAY_PRESENT_ROTATE_0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t resolve_panel_interface(
    const esp_display_present_target_config_t *config,
    esp_display_present_panel_interface_t *out_panel_interface)
{
    switch (config->hw.panel_type) {
    case ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI:
        *out_panel_interface = ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI;
        return ESP_OK;
    case ESP_DISPLAY_PRESENT_PANEL_RGB:
        *out_panel_interface = ESP_DISPLAY_PRESENT_PANEL_IF_RGB;
        return ESP_OK;
    case ESP_DISPLAY_PRESENT_PANEL_IO:
        *out_panel_interface = ESP_DISPLAY_PRESENT_PANEL_IF_OTHER;
        return ESP_OK;
    case ESP_DISPLAY_PRESENT_PANEL_AUTO:
        if (config->hw.io == NULL) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        *out_panel_interface = ESP_DISPLAY_PRESENT_PANEL_IF_OTHER;
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t esp_display_present_profile_resolve(
    const esp_display_present_target_config_t *config,
    esp_display_present_panel_interface_t *out_panel_interface,
    esp_display_present_profile_t *out_profile)
{
    if (config == NULL || out_panel_interface == NULL || out_profile == NULL ||
            config->fb.mode < ESP_DISPLAY_PRESENT_MODE_NONE ||
            config->fb.mode > ESP_DISPLAY_PRESENT_MODE_AUTO ||
            (config->hw.rotation != ESP_DISPLAY_PRESENT_ROTATE_0 &&
             config->hw.rotation != ESP_DISPLAY_PRESENT_ROTATE_90 &&
             config->hw.rotation != ESP_DISPLAY_PRESENT_ROTATE_180 &&
             config->hw.rotation != ESP_DISPLAY_PRESENT_ROTATE_270)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = resolve_panel_interface(config, out_panel_interface);
    if (ret != ESP_OK) {
        return ret;
    }
    const bool te_enabled = config->hw.te_enabled &&
                            esp_display_present_te_sync_is_enabled(&config->hw.te_sync);
    ret = resolve_profile(config->fb.mode, *out_panel_interface, te_enabled,
                          out_profile);
    if (ret != ESP_OK) {
        return ret;
    }
    return validate_profile(out_profile, *out_panel_interface, te_enabled,
                            config->hw.rotation);
}

esp_err_t esp_display_present_get_required_frame_buffer_count(
    const esp_display_present_target_config_t *config,
    uint8_t *out_count)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (config == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_AUTO) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_display_present_panel_interface_t panel_interface;
    esp_display_present_profile_t profile;
    esp_err_t ret = esp_display_present_profile_resolve(
                        config, &panel_interface, &profile);
    if (ret == ESP_OK) {
        *out_count = profile.frame_buffer_count;
    }
    return ret;
}
