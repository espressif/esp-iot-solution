/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "config/present_mode.h"

typedef struct {
    esp_display_present_render_contract_t contract;
    uint8_t frame_buffers;
} mode_profile_t;
/* One table for explicit framebuffer modes at both configuration boundaries. */
static const mode_profile_t profiles[] = {
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, 1},
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL, 2},
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL, 3},
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT, 2},
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, 3},
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, 0},
    [ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL] = {ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, 2},
};
static esp_display_present_present_mode_t io_auto(bool te_enabled)
{
    return te_enabled ? ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC : ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
}

esp_err_t present_mode_resolve_target(const esp_display_present_target_config_t *target, present_mode_resolution_t *out)
{
    if (!target || !out || (target->hw.rotation != 0 && target->hw.rotation != 90 && target->hw.rotation != 180 && target->hw.rotation != 270) ||
            target->fb.mode < ESP_DISPLAY_PRESENT_MODE_NONE || target->fb.mode > ESP_DISPLAY_PRESENT_MODE_AUTO) {
        return ESP_ERR_INVALID_ARG;
    }
    if (target->hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_AUTO && !target->hw.io) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (target->hw.panel_type < ESP_DISPLAY_PRESENT_PANEL_AUTO || target->hw.panel_type > ESP_DISPLAY_PRESENT_PANEL_IO) {
        return ESP_ERR_INVALID_ARG;
    }
    bool fb = target->hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_RGB || target->hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI;
    bool te = target->hw.te_enabled && target->hw.te_sync.gpio_num >= 0;
    static const esp_display_present_present_mode_t public_modes[] = {
        ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL,
        ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC,
        ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO,
    };
    esp_display_present_present_mode_t mode = public_modes[target->fb.mode];
    if (mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO) {
        if (fb) {
            unsigned count = target->fb.frame_buffer_count ? target->fb.frame_buffer_count : 3;
            if (count < 2) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            mode = count >= 3 ? ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL : ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL;
        } else {
            mode = io_auto(te);
        }
    }
    if ((mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC && (fb || !te)) ||
            (!fb && mode != ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE && mode != ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (fb && mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    *out = (present_mode_resolution_t) {
        .kind = fb ? ESP_DISPLAY_PRESENT_ENGINE_FB : mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC ? ESP_DISPLAY_PRESENT_ENGINE_TE : ESP_DISPLAY_PRESENT_ENGINE_REGION,
        .engine_mode = mode,
        .contract = profiles[mode].contract,
        .frame_buffers = fb ? profiles[mode].frame_buffers : 0,
        .active_writes = false,
    };
    return ESP_OK;
}

esp_err_t present_mode_resolve_engine(const esp_display_present_engine_config_t *input, esp_display_present_plan_defaults_t defaults,
                                      bool te_enabled, esp_display_present_engine_config_t *out)
{
    if (!input || !out || input->backend_kind < ESP_DISPLAY_PRESENT_ENGINE_REGION || input->backend_kind > ESP_DISPLAY_PRESENT_ENGINE_FB ||
            input->mode < ESP_DISPLAY_PRESENT_MODE_DEFAULT || input->mode > ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO ||
            defaults < ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS || defaults > ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = *input;
    if (defaults == ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS && input->backend_kind != ESP_DISPLAY_PRESENT_ENGINE_FB && out->mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO) {
        out->mode = io_auto(te_enabled);
    }
    esp_display_present_present_mode_t mode = out->mode;
    if (mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO) {
        if (input->backend_kind == ESP_DISPLAY_PRESENT_ENGINE_FB) {
            switch (input->backend.buffer_count) {
            case 1: return ESP_ERR_NOT_SUPPORTED;
            case 2: mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL; break;
            case 3: mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL; break;
            default: return ESP_ERR_INVALID_ARG;
            }
        } else {
            mode = input->backend.te_window_us && input->backend.te_budget_us ? ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC : ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
        }
    }
    out->mode = mode;
    if (mode == ESP_DISPLAY_PRESENT_MODE_DEFAULT) {
        if (input->backend_kind == ESP_DISPLAY_PRESENT_ENGINE_FB &&
                (input->backend.buffer_count < 2 || input->backend.active_writes)) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        return ESP_OK;
    }
    if (mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE) {
        out->backend.render_contract = profiles[mode].contract;
        if (input->backend_kind == ESP_DISPLAY_PRESENT_ENGINE_FB) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        out->backend_kind = ESP_DISPLAY_PRESENT_ENGINE_REGION;
        return ESP_OK;
    }
    if (mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC) {
        if (input->backend_kind == ESP_DISPLAY_PRESENT_ENGINE_FB) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        out->backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE;
        out->backend.render_contract = profiles[mode].contract;
        return ESP_OK;
    }
    if (input->backend_kind != ESP_DISPLAY_PRESENT_ENGINE_FB) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    out->backend.buffer_count = profiles[mode].frame_buffers;
    if (input->backend.ext_bufs && input->backend.buffer_count != out->backend.buffer_count) {
        return ESP_ERR_INVALID_ARG;
    }
    out->backend.render_contract = profiles[mode].contract;
    return ESP_OK;
}
