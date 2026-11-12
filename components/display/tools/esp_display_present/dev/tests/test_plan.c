/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "config/present_plan.h"
#include "core/present_engine.h"
#include "resources/present_memory.h"
#include "../../src/platform/te/present_te_timer.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
static unsigned attachments;
static esp_err_t attach(void *ctx, esp_display_present_panel_event_fn cb, void *user)
{
    (void)ctx;
    (void)cb;
    (void)user;
    attachments++;
    return ESP_OK;
}
static esp_display_present_tx_result_t submit(void *ctx, const esp_display_present_surface_t *s, const esp_display_present_rect_t *r, uint64_t c)
{
    (void)ctx;
    (void)s;
    (void)r;
    (void)c;
    return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL
    };
}
static esp_err_t quiet(void *ctx, uint32_t t)
{
    (void)ctx;
    (void)t;
    return ESP_OK;
}
static esp_display_present_plan_config_t config(esp_display_present_engine_backend_t kind)
{
    return (esp_display_present_plan_config_t) {
        .presenter = {.backend_kind = kind, .backend = {.width = 480, .height = 480, .tile_rows = 16, .buffer_count = 2, .max_frames = 4,
                                                        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {NULL, attach, submit, quiet}, .te_window_us = 1000, .te_budget_us = 100
                                                       }
                     },
        .output = {.internal_dma = true, .ext_mem = true, .tight_stride = true}
    };
}
static void modes(void)
{
    struct {
        esp_display_present_present_mode_t mode;
        size_t images;
        esp_display_present_render_contract_t contract;
    } cases[] = {
        {ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL},
        {ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL}, {ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT},
        {ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION}, {ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_FB);
        c.presenter.mode = cases[i].mode;
        c.presenter.backend.active_writes = true;
        esp_display_present_resolved_plan_t p;
        assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
        assert(p.images.count == cases[i].images && p.normalized.backend.render_contract == cases[i].contract);
        assert(p.logical.count == 0);
        c.resources.image_count = cases[i].images == 2 ? 3 : 2;
        assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    }
    esp_display_present_resolved_plan_t p;
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && !p.images.count && p.tiles.count == 2);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO;
    c.presenter.backend.te_window_us = 0;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.normalized.backend_kind == ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.defaults = ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS;
    c.te_enabled = true;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
    c.presenter.backend.te_window_us = 1000;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.images.count == 1 && !p.pending_runtime);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_FB);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.images.count == 2);
    c.defaults = ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.images.count == 2);
    c.presenter.backend.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90;
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.logical.count == 1);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.defaults = ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS;
    c.presenter.backend.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.conversion.count == p.tiles.count);
}
/* Golden cases for the constraints shared by plan and backend create.
 * These passed before the extraction and must keep the same error codes;
 * layer-specific divergences (e.g. TE timing NOT_SUPPORTED here vs
 * INVALID_ARG at TE create) are pinned elsewhere and not unified. */
static void shared_backend_constraints(void)
{
    esp_display_present_resolved_plan_t p;
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
    c.presenter.backend.width = 0;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.rotation = (esp_display_present_engine_rotation_t)4;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.format = (esp_display_present_engine_pixel_format_t)9;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.draw_order = (esp_display_present_byte_order_t)9;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.panel_order = (esp_display_present_byte_order_t)9;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.format = ESP_DISPLAY_PRESENT_FORMAT_RGB888;
    c.presenter.backend.panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.max_frames = 0;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.max_frames = 33;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    /* Peak budget: async REGION plans a full conversion pool even when the
     * identity path will not allocate it yet. These are fixed baseline
     * numbers for this config (480x480, 16 tile rows, RGB565, 2 buffers):
     * they must not shrink with the backend's current allocation. */
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE;
    c.presenter.backend.async_copy = true;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
    assert(p.conversion.count == p.tiles.count && p.conversion.count == 2);
    assert(p.tiles.bytes_each == 15360 && p.tiles.total_bytes == 30720);
    assert(p.conversion.bytes_each == 15360 && p.conversion.total_bytes == 30720);
    assert(p.pixel_bytes == 61440 && p.coverage_bytes == 0);
    assert(p.reserved_bytes == 0 && p.stack_bytes == 4096);
    assert(p.metadata_bytes == present_presenter_memory(&p.normalized) + present_region_memory(&p.normalized.backend));
    assert(p.required_bytes == p.pixel_bytes + p.coverage_bytes + p.metadata_bytes + p.stack_bytes + p.reserved_bytes);
    size_t required = p.required_bytes;
    c.total_budget_bytes = required - 1;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NO_MEM);
    c.total_budget_bytes = required;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.required_bytes == required);
}

static void topology_and_budget(void)
{
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_FB);
    c.presenter.backend.width = 1024;
    c.presenter.backend.height = 600;
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL;
    c.presenter.backend.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90;
    esp_display_present_resolved_plan_t p;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
    assert(p.physical_layout.width == 600 && p.physical_layout.height == 1024);
    assert(p.topology == ESP_DISPLAY_PRESENT_TOPOLOGY_TILE_TO_PHYSICAL && p.logical.count == 0);
    assert(p.images.total_bytes == 3686400 && p.tiles.total_bytes == 65536 && p.coverage_bytes == 76800);
    assert(!(p.pending_runtime & ESP_DISPLAY_PRESENT_PLAN_NEEDS_ROTATION_TOPOLOGY));
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.logical.count == 1 && !p.tiles.count);
    assert(p.pixel_bytes == 4915200 && p.logical.total_bytes == 1228800);
    c.pixel_budget_bytes = p.pixel_bytes - 1;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NO_MEM && p.pixel_bytes == 0);
    c.pixel_budget_bytes = 0;
    c.resources.logical_count = 2;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
    c.resources.logical_count = 1;
    /* FULL/DIRECT byte-order conversion uses the single logical canvas topology. */
    c.presenter.backend.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0;
    c.presenter.backend.draw_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE;
    c.presenter.backend.panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.topology == ESP_DISPLAY_PRESENT_TOPOLOGY_LOGICAL_TO_PHYSICAL && p.logical.count == 1 && !p.pending_runtime);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.backend.width = INT_MAX;
    c.presenter.backend.height = INT_MAX;
    c.presenter.backend.tile_rows = INT_MAX;
    c.resources.tile_count = 16;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_SIZE);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_TE);
    c.resources.image_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    c.output.ext_mem = false;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
}
/* Independent per-pixel mapping checks rectangle coverage in all rotations. */
static void map(unsigned x, unsigned y, unsigned w, unsigned h, unsigned rot, unsigned *px, unsigned *py)
{
    if (rot == 0) {
        *px = x;
        *py = y;
    } else if (rot == 1) {
        *px = h - 1 - y;
        *py = x;
    } else if (rot == 2) {
        *px = w - 1 - x;
        *py = h - 1 - y;
    } else {
        *px = y;
        *py = w - 1 - x;
    }
}
static void alignment(void)
{
    for (unsigned rot = 0; rot < 4; rot++) {
        esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
        c.presenter.backend.width = 12;
        c.presenter.backend.height = 8;
        c.presenter.backend.tile_rows = 4;
        c.presenter.backend.rotation = rot;
        c.output.x_alignment = 2;
        c.output.w_align = 4;
        c.output.y_alignment = 2;
        c.output.h_align = 2;
        esp_display_present_resolved_plan_t p;
        assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
        esp_display_present_rect_t dirty = {3, 1, 3, 3}, r, a;
        assert(esp_display_present_plan_render_area(&p, &dirty, &r, &a) == ESP_OK);
        assert(a.x % 2 == 0 && a.width % 4 == 0 && a.y % 2 == 0 && a.height % 2 == 0);
        esp_display_present_rect_t edge = {11, 7, 1, 1}, er, ep;
        assert(esp_display_present_plan_render_area(&p, &edge, &er, &ep) == ESP_OK);
        assert(ep.x + ep.width <= p.physical_layout.width && ep.y + ep.height <= p.physical_layout.height);
        for (unsigned y = dirty.y; y < dirty.y + dirty.height; y++)
            for (unsigned x = dirty.x; x < dirty.x + dirty.width; x++) {
                unsigned px, py;
                map(x, y, 12, 8, rot, &px, &py);
                assert(px >= a.x && px < a.x + a.width && py >= a.y && py < a.y + a.height);
                assert(x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height);
            }
    }
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    esp_display_present_resolved_plan_t p;
    c.presenter.backend.width = 7;
    c.output.w_align = 4;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.presenter.backend.format = ESP_DISPLAY_PRESENT_FORMAT_RGB888;
    c.presenter.backend.width = 5;
    c.output.stride_align = 64;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED); /* tight row cannot be padded */
    c.output.tight_stride = false;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED); /* non-tight padded pool layout rejected */
}
static void creation(void)
{
    esp_display_present_plan_config_t partial = config(ESP_DISPLAY_PRESENT_ENGINE_FB);
    partial.output.full_only = true;
    esp_display_present_resolved_plan_t plan;
    assert(esp_display_present_plan_resolve(&partial, &plan) == ESP_OK);
    esp_display_present_rect_t dirty = {3, 4, 5, 6}, render, physical;
    assert(esp_display_present_plan_render_area(&plan, &dirty, &render, &physical) == ESP_OK);
    assert(!memcmp(&render, &dirty, sizeof(dirty)));
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_FB);
    c.presenter.mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL;
    esp_display_present_engine_t *presenter = NULL;
    unsigned before = attachments;
    assert(esp_display_present_engine_create_planned(&c, &presenter) == ESP_OK && attachments == before + 1);
    assert(esp_display_present_engine_stop(presenter, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(presenter) == ESP_OK);
    before = attachments;
    c.resources.tile_count = 1;
    assert(esp_display_present_engine_create_planned(&c, &presenter) == ESP_OK && attachments == before + 1);
    assert(esp_display_present_engine_stop(presenter, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(presenter) == ESP_OK);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_TE);
    before = attachments;
    assert(esp_display_present_engine_create_planned(&c, &presenter) == ESP_OK && attachments == before + 1);
    assert(esp_display_present_engine_stop(presenter, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(presenter) == ESP_OK);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    assert(esp_display_present_engine_create_planned(&c, &presenter) == ESP_OK);
    assert(esp_display_present_engine_stop(presenter, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(presenter) == ESP_OK);
    /* Internal single-resolve entry reports the plan it actually used. */
    c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    esp_display_present_resolved_plan_t expected, actual;
    assert(esp_display_present_plan_resolve(&c, &expected) == ESP_OK);
    assert(present_engine_create_with_plan(&c, &actual, &presenter) == ESP_OK && presenter);
    assert(actual.required_bytes == expected.required_bytes && actual.pixel_bytes == expected.pixel_bytes &&
           actual.tiles.count == expected.tiles.count && actual.conversion.total_bytes == expected.conversion.total_bytes &&
           actual.normalized.backend.buffer_count == expected.normalized.backend.buffer_count);
    assert(esp_display_present_engine_stop(presenter, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(presenter) == ESP_OK);
    assert(present_engine_create_with_plan(&c, NULL, &presenter) == ESP_ERR_INVALID_ARG);
    assert(present_engine_create_with_plan(NULL, &actual, &presenter) == ESP_ERR_INVALID_ARG && !presenter);
}
static void reverse_band_alignment(void)
{
    for (unsigned rotation = 0; rotation < 4; ++rotation) {
        esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
        c.presenter.backend.rotation = rotation;
        c.output.x_alignment = c.output.y_alignment = 4;
        c.output.w_align = c.output.h_align = 2;
        esp_display_present_resolved_plan_t p;
        assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
        esp_display_present_rect_t dirty = {3, 3, 6, 18}, render, physical;
        assert(esp_display_present_plan_render_area(&p, &dirty, &render, &physical) == ESP_OK);
        assert(physical.width % 4 == 0 && physical.height % 4 == 0);
        /* Every forward/reversed four-row band retains a legal origin. */
        assert((physical.x + physical.width - 4) % 4 == 0);
        assert((physical.y + physical.height - 4) % 4 == 0);
    }
}
static void external_dma_alignment(void)
{
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_REGION);
    c.resources.tile_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    c.output.ext_mem = true;
    c.output.ext_len_align = 64;
    esp_display_present_resolved_plan_t p;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
    assert(p.output.w_align == 32);
    esp_display_present_rect_t dirty = {1, 1, 1, 1}, render, physical;
    assert(esp_display_present_plan_render_area(&p, &dirty, &render, &physical) == ESP_OK);
    assert((size_t)physical.width * physical.height * 2 % 64 == 0);
    c = config(ESP_DISPLAY_PRESENT_ENGINE_TE);
    c.resources.image_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    c.output.ext_mem = true;
    c.output.ext_len_align = 64;
    c.presenter.backend.width = 31;
    c.presenter.backend.height = 31;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
}
static esp_err_t test_gate_admit(void *user, size_t bytes, uint64_t after, uint64_t *edge)
{
    (void)user;
    (void)bytes;
    (void)after;
    (void)edge;
    return ESP_ERR_TIMEOUT;
}
static void timing_model(void)
{
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_TE);
    c.presenter.backend.te_timing = (esp_display_present_te_timing_t) {
        .period_us = 100, .scan_us = 101
    };
    c.presenter.backend.te_budget_us = 50;
    esp_display_present_resolved_plan_t p;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
    c.presenter.backend.te_timing.scan_us = 80;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
    c.presenter.backend.te_gate.admit = test_gate_admit;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_INVALID_ARG);
    c.presenter.backend.te_timing.period_us = 0;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
}
/* Native timing allocates a framework timer wrapper even when the incoming
 * callback is NULL: Presenter installs its own progress callback at create. */
static void timing_timer_budget(void)
{
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_TE);
    esp_display_present_resolved_plan_t fixed, timed, gate;
    assert(esp_display_present_plan_resolve(&c, &fixed) == ESP_OK);
    c.presenter.backend.te_timing = (esp_display_present_te_timing_t) {
        .period_us = 1000, .scan_us = 800
    };
    assert(esp_display_present_plan_resolve(&c, &timed) == ESP_OK);
    assert(timed.metadata_bytes == fixed.metadata_bytes + present_te_timer_memory());
    assert(timed.required_bytes == fixed.required_bytes + present_te_timer_memory());
    c.total_budget_bytes = timed.required_bytes - 1;
    assert(esp_display_present_plan_resolve(&c, &timed) == ESP_ERR_NO_MEM);
    c.total_budget_bytes = 0;
    c.presenter.backend.te_timing.period_us = 0;
    c.presenter.backend.te_gate.admit = test_gate_admit;
    assert(esp_display_present_plan_resolve(&c, &gate) == ESP_OK);
    assert(gate.metadata_bytes == fixed.metadata_bytes);
}
static void total_budget(void)
{
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_FB);
    c.extra_bytes = 12345;
    esp_display_present_resolved_plan_t p;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK);
    assert(p.metadata_bytes && p.required_bytes == p.pixel_bytes + p.coverage_bytes + p.metadata_bytes + p.stack_bytes + 12345);
    c.total_budget_bytes = p.required_bytes - 1;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NO_MEM);
}
static esp_err_t fake_caps(void *ctx, esp_display_present_executor_caps_t *c)
{
    (void)ctx;
    *c = (esp_display_present_executor_caps_t) {
        .format_mask = 3, .rotation_mask = 1, .base_alignment = 64, .stride_align = 64
    };
    return ESP_OK;
}
static esp_err_t forbidden_probe(void *ctx, const esp_display_present_copy_request_t *r, esp_display_present_executor_reason_t *reason)
{
    (void)ctx;
    (void)r;
    (void)reason;
    assert(0);
    return ESP_FAIL;
}
static esp_err_t forbidden_execute(void *ctx, const esp_display_present_copy_request_t *r)
{
    (void)ctx;
    (void)r;
    assert(0);
    return ESP_FAIL;
}
static esp_err_t fake_destroy(void *ctx)
{
    (void)ctx;
    return ESP_OK;
}
static void executor(void)
{
    const esp_display_present_executor_ops_t ops = {fake_caps, forbidden_probe, forbidden_execute, fake_destroy};
    esp_display_present_executor_t fake = {&ops, NULL}, *cpu;
    assert(esp_display_present_executor_cpu_create(&cpu) == ESP_OK);
    esp_display_present_executor_dispatch_t d = {&fake, cpu};
    esp_display_present_plan_config_t c = config(ESP_DISPLAY_PRESENT_ENGINE_TE);
    c.executor = &d;
    c.presenter.backend.async_copy = true;
    c.presenter.backend.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90;
    esp_display_present_resolved_plan_t p;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_OK && p.preferred_executor == cpu && p.probe_addrs);
    d.fallback = NULL;
    assert(esp_display_present_plan_resolve(&c, &p) == ESP_ERR_NOT_SUPPORTED);
    assert(esp_display_present_executor_destroy(cpu) == ESP_OK);
}
int main(void)
{
    reverse_band_alignment();
    external_dma_alignment();
    timing_model();
    timing_timer_budget();
    total_budget();
    modes();
    shared_backend_constraints();
    topology_and_budget();
    alignment();
    creation();
    executor();
    puts("plan: mode compatibility, topology, budgets, alignment, pure capabilities and fail-before-attach passed");
    return 0;
}
