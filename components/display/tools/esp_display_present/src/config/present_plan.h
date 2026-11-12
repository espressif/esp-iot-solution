/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "config/present_engine_config.h"
#include "core/present_engine_types.h"
#include "config/present_mode.h"
#include "pixel/present_executor.h"
#include "backend/present_output_caps.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_DISPLAY_PRESENT_MEMORY_INTERNAL_DMA,
    ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL,
} esp_display_present_plan_memory_t;
typedef enum {
    ESP_DISPLAY_PRESENT_TOPOLOGY_REGION,
    ESP_DISPLAY_PRESENT_TOPOLOGY_TILE_TO_PHYSICAL,
    ESP_DISPLAY_PRESENT_TOPOLOGY_DIRECT_SURFACE,
    ESP_DISPLAY_PRESENT_TOPOLOGY_LOGICAL_TO_PHYSICAL,
} esp_display_present_plan_topology_t;
/* tiles are presenter input buffers, not frontend buffers or TE compose images.
 * Independent pool quantities; zero selects defaults. Explicit overrides must
 * agree with named mode image counts. Memory kind is explicit; no implicit staging allocation. */
typedef struct {
    size_t tile_count, image_count, logical_count, tx_capacity;
    esp_display_present_plan_memory_t tile_memory, image_memory, logical_memory;
} esp_display_present_plan_resources_t;
typedef struct {
    esp_display_present_engine_config_t presenter;
    esp_display_present_plan_defaults_t defaults;
    bool te_enabled; /* V1 AUTO only; does not invent TE timing measurements. */
    esp_display_present_plan_resources_t resources;
    esp_display_present_output_caps_t output;
    /* Borrowed dispatcher and executors must outlive the created presenter.
     * Resolve queries caps only; actual address probe remains mandatory. */
    const esp_display_present_executor_dispatch_t *executor;
    size_t extra_bytes; /* Driver, renderer and allocator overhead reservation. */
    size_t total_budget_bytes; /* 0 unlimited; framework requests + caller reservation. */
    size_t pixel_budget_bytes; /* 0 unlimited; includes borrowed pixel buffers. */
} esp_display_present_plan_config_t;
typedef struct {
    size_t count, bytes_each, total_bytes;
    esp_display_present_layout_t layout;
    esp_display_present_plan_memory_t memory;
    bool borrowed;
} esp_display_present_plan_pool_t;
/* Bits indicate missing runtime paths, not malformed configurations. */
enum {
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_FB_TILE_POOL = 1u << 0,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_ROTATION_TOPOLOGY = 1u << 1,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_TE_POOL = 1u << 2,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_MEMORY_PATH = 1u << 3,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_PADDED_LAYOUT = 1u << 4,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_REGION_CONTRACT = 1u << 5,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_DYNAMIC_TE = 1u << 6,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_TE_PIPELINE = 1u << 7,
    ESP_DISPLAY_PRESENT_PLAN_NEEDS_TX_LIMIT = 1u << 8,
};
/* Memory accounting uses three distinct quantities; do not conflate them.
 * - Peak resource budget (pixel_bytes / required_bytes): an estimate over
 *   every supported path, including borrowed buffers and the caller
 *   reservation. An async REGION plan counts the full conversion pool even
 *   when the identity path has not allocated any of it.
 * - Pool planned capacity (each pool's count / bytes_each / total_bytes):
 *   how many buffers of what size in which memory a path may need at most.
 * - Current actual allocation: what a backend has allocated and holds right
 *   now. Lazy paths (e.g. conversion tiles after a runtime rotation) can be
 *   below planned capacity and can still fail with ESP_ERR_NO_MEM later.
 * Passing a budget check reserves no heap memory. */
typedef struct {
    esp_display_present_engine_config_t normalized;
    esp_display_present_plan_topology_t topology;
    esp_display_present_layout_t logical_layout, physical_layout;
    esp_display_present_plan_pool_t tiles, images, logical, conversion;
    esp_display_present_output_caps_t output;
    size_t tx_capacity, record_capacity, base_alignment;
    size_t pixel_bytes, coverage_bytes; /* Peak pixel budget. coverage_bytes budgets a worst-case lazy bitmap, not a resident allocation. */
    size_t metadata_bytes, stack_bytes, reserved_bytes, required_bytes;
    /* reserved_bytes is exactly the caller's extra_bytes from the plan
     * config; it is not a conversion-pool or heap reservation.
     * required_bytes is the peak budget: pixel + coverage + framework
     * metadata + requested task stacks + caller reservation. Add
     * driver/renderer/allocator overhead through extra_bytes. Host pthread
     * stack size is OS-defined. */
    const esp_display_present_executor_t *preferred_executor;
    esp_display_present_executor_reason_t executor_reason;
    bool probe_addrs;
    uint32_t pending_runtime;
} esp_display_present_resolved_plan_t;

/* Pure planning: no allocation/attach/driver calls; get_caps only. Invalid or
 * unsupported physical constraints return errors; valid future topologies are
 * described with pending_runtime bits and cannot be created by today's bridge. */
esp_err_t esp_display_present_plan_resolve(const esp_display_present_plan_config_t *config, esp_display_present_resolved_plan_t *plan);
/* Dirty L -> aligned P -> required render L, before rasterization. No clipping
 * of an expanded physical edge that would violate alignment. No pixel reads. */
esp_err_t esp_display_present_plan_render_area(const esp_display_present_resolved_plan_t *plan, const esp_display_present_rect_t *dirty,
                                               esp_display_present_rect_t *render, esp_display_present_rect_t *physical);
/* Re-resolves rather than trusting a caller-mutated plan. Pending runtime paths
 * reject BEFORE any allocation or callback attachment; no legacy fallback. */
esp_err_t esp_display_present_engine_create_planned(const esp_display_present_plan_config_t *config, esp_display_present_engine_t **out);
/* Internal single-resolve entry: resolves once, reports the exact plan that
 * was used for creation in out_plan (set after a successful resolve, before
 * creation is attempted), and creates from it. Callers that retry with a
 * mutated config must call again so out_plan always matches the final
 * attempt. This does not remove the re-validation in
 * esp_display_present_engine_create_planned(), which stays the external
 * boundary for untrusted plan configs. */
esp_err_t present_engine_create_with_plan(const esp_display_present_plan_config_t *config,
                                          esp_display_present_resolved_plan_t *out_plan,
                                          esp_display_present_engine_t **out);
#ifdef __cplusplus
}
#endif
