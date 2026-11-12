/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_display_present_config.h"
#include "config/present_engine_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Select default pool/mode policy, not a ceiling on supported capabilities. */
typedef enum {
    ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS,
    ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS,
} esp_display_present_plan_defaults_t;

/* Mode policy only: no heap probing, hardware calls or resource allocation.
 * RGB/DSI scanout requires at least two FBs and rejects NONE.
 * frame_buffers counts driver FBs, not TE composition images or draw tiles. */
typedef struct {
    esp_display_present_engine_backend_t kind;
    esp_display_present_present_mode_t engine_mode;
    esp_display_present_render_contract_t contract;
    uint8_t frame_buffers;
    bool active_writes;
} present_mode_resolution_t;
esp_err_t present_mode_resolve_target(const esp_display_present_target_config_t *, present_mode_resolution_t *);
esp_err_t present_mode_resolve_engine(const esp_display_present_engine_config_t *, esp_display_present_plan_defaults_t,
                                      bool te_enabled, esp_display_present_engine_config_t *);

#ifdef __cplusplus
}
#endif
