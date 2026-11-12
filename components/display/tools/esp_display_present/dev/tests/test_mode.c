/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "config/present_mode.h"
#include <assert.h>

static void public_profiles(void)
{
    const struct {
        esp_display_present_mode_t mode;
        unsigned count;
        esp_display_present_render_contract_t contract;
    } cases[] = {
        {ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL},
        {ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL},
        {ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT},
        {ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION},
        {ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION},
        {ESP_DISPLAY_PRESENT_MODE_AUTO, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION},
    };
    for (unsigned panel = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI; panel <= ESP_DISPLAY_PRESENT_PANEL_RGB; ++panel) {
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            esp_display_present_target_config_t target = {.hw = {.panel_type = panel}, .fb = {.mode = cases[i].mode}};
            present_mode_resolution_t mode;
            assert(present_mode_resolve_target(&target, &mode) == ESP_OK);
            assert(mode.kind == ESP_DISPLAY_PRESENT_ENGINE_FB);
            assert(mode.frame_buffers == cases[i].count);
            assert(mode.contract == cases[i].contract);
            assert(!mode.active_writes);
        }
    }
    for (unsigned kind = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI; kind <= ESP_DISPLAY_PRESENT_PANEL_RGB; ++kind) {
        esp_display_present_target_config_t target = {.hw = {.panel_type = kind}, .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_NONE}};
        present_mode_resolution_t mode;
        assert(present_mode_resolve_target(&target, &mode) == ESP_ERR_NOT_SUPPORTED);
        target.fb.mode = ESP_DISPLAY_PRESENT_MODE_AUTO;
        for (unsigned count = 1; count <= 3; ++count) {
            target.fb.frame_buffer_count = count;
            esp_err_t error = present_mode_resolve_target(&target, &mode);
            assert(error == (count == 1 ? ESP_ERR_NOT_SUPPORTED : ESP_OK));
            if (count > 1) {
                assert(mode.frame_buffers == count && mode.kind == ESP_DISPLAY_PRESENT_ENGINE_FB);
                assert(mode.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
            }
        }
    }
    esp_display_present_target_config_t target = {.hw = {.panel_type = ESP_DISPLAY_PRESENT_PANEL_IO, .te_enabled = true,
                                                         .te_sync = {.gpio_num = 0}
                                                        }, .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_AUTO}
                                                 };
    present_mode_resolution_t mode;
    assert(present_mode_resolve_target(&target, &mode) == ESP_OK && mode.kind == ESP_DISPLAY_PRESENT_ENGINE_TE && !mode.frame_buffers);
    target.hw.te_sync.gpio_num = -1;
    assert(present_mode_resolve_target(&target, &mode) == ESP_OK && mode.kind == ESP_DISPLAY_PRESENT_ENGINE_REGION);
    target.fb.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC;
    assert(present_mode_resolve_target(&target, &mode) == ESP_ERR_NOT_SUPPORTED);
    target.fb.mode = ESP_DISPLAY_PRESENT_MODE_NONE;
    target.hw.panel_type = ESP_DISPLAY_PRESENT_PANEL_AUTO;
    assert(present_mode_resolve_target(&target, &mode) == ESP_ERR_NOT_SUPPORTED);
    target.hw.io = (esp_lcd_panel_io_handle_t)&target;
    assert(present_mode_resolve_target(&target, &mode) == ESP_OK && mode.kind == ESP_DISPLAY_PRESENT_ENGINE_REGION);
    target.hw.rotation = (esp_display_present_rotation_t)45;
    assert(present_mode_resolve_target(&target, &mode) == ESP_ERR_INVALID_ARG);
    assert(present_mode_resolve_target(NULL, &mode) == ESP_ERR_INVALID_ARG);
}
static void engine_policies(void)
{
    esp_display_present_engine_config_t input = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB,
                                                 .mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE,
                                                 .backend = {.buffer_count = 2, .active_writes = true}
                                                };
    esp_display_present_engine_config_t out;
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS, false, &out) == ESP_ERR_NOT_SUPPORTED);
    input.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO;
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS, false, &out) == ESP_OK);
    assert(out.backend.buffer_count == 2 && out.mode == ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL);
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS, false, &out) == ESP_OK);
    assert(out.backend.buffer_count == 2);
    input.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_REGION;
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS, true, &out) == ESP_OK);
    assert(out.backend_kind == ESP_DISPLAY_PRESENT_ENGINE_REGION); /* GPIO capability does not invent timing in current policy. */
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS, true, &out) == ESP_OK);
    assert(out.backend_kind == ESP_DISPLAY_PRESENT_ENGINE_TE);
    input.backend.te_window_us = 1000;
    input.backend.te_budget_us = 100;
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS, false, &out) == ESP_OK);
    assert(out.backend_kind == ESP_DISPLAY_PRESENT_ENGINE_TE);
    input.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB;
    input.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL;
    void *buffers[2] = {0};
    input.backend.ext_bufs = buffers;
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS, false, &out) == ESP_ERR_INVALID_ARG);
    input.backend.active_writes = false;
    input.mode = ESP_DISPLAY_PRESENT_MODE_DEFAULT;
    input.backend.render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT;
    assert(present_mode_resolve_engine(&input, ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS, false, &out) == ESP_OK);
    assert(out.backend.ext_bufs == buffers && out.backend.buffer_count == 2 && out.backend.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT);
}
int main(void)
{
    public_profiles();
    engine_policies();
    return 0;
}
