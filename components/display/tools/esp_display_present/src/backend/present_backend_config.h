/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_types.h"
#include "platform/te/present_te_timing.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Advanced override. The default presenter_create path does not use this: it
 * binds the internal GPIO sampler after create and then clears any placeholder
 * window. Borrowed until stop; task-side admit
 * runs outside backend locks and may wait for a bounded hardware window.
 * TIMEOUT means no admission (retry), never sticky fault. A successful return
 * supplies the actual selected edge; rejected transfers must wait for a newer
 * edge. done is ISR-safe and observes real completion, or definite rejection.
 * No source ownership, image pools or frame lifecycle belongs to this policy. */
typedef struct {
    esp_err_t (*admit)(void *, size_t transfer_bytes, uint64_t after_edge, uint64_t *edge);
    void (*start)(void *);
    void (*done)(void *, esp_err_t);
    void *user;
} esp_display_present_te_gate_t;

struct esp_display_present_backend_config {
    uint32_t width, height;     /* Logical dimensions seen by the producer. */
    uint32_t tile_rows;         /* Presenter input tile height in PARTITION modes. */
    size_t buffer_count;        /* 1..16 draw/frame buffers. */
    size_t max_frames;          /* 1..32 bounded frame/receipt records. */
    esp_display_present_engine_pixel_format_t format;
    esp_display_present_byte_order_t draw_order;
    esp_display_present_byte_order_t panel_order;
    esp_display_present_engine_rotation_t rotation;    /* Logical-to-physical clockwise rotation. */
    bool in_place;      /* True: swap byte order in-place on tight stride and revert on rejection. */
    void *const *ext_bufs; /* Optional caller/driver-owned FB array (TE/FB). */
    void *scan_buf; /* Optional member of ext_bufs already scanning; not writable until retired. */
    size_t ext_bytes;  /* Capacity of each external buffer. */
    esp_display_present_panel_adapter_t panel;  /* Borrowed until successful stop. */
    /** TE only: measured safe window from edge and worst-case whole-frame
     * transfer budget (including dispatch overhead). Both nonzero, budget < window.
     * region ignores these fields. No hardware timing is inferred automatically. */
    uint32_t te_window_us;
    uint32_t te_budget_us;
    /** FB: 1..3 framebuffers in buffer_count; PARTITION uses the input tile pool.
     * FULL/DIRECT leases expose the whole candidate; submit coordinates index
     * that full surface (not scratch origin). Zero defaults to PARTITION. */
    esp_display_present_render_contract_t render_contract;
    /** Single-FB is permitted only with explicit acceptance of active scanout
     * writes/tearing. Cancel cannot roll back pixels already visible. */
    bool active_writes;
    /** Enables the pixel worker for copy, repair and conversion jobs.
     * Defaults retain synchronous CPU operation.
     * Executor must obey pixel_execute's return-means-access-ended contract.
     * region async_copy requires separate conversion storage (no in_place). */
    bool async_copy;
    esp_display_present_pixel_execute_fn copy_execute;
    void *copy_user;
    /** Optional wake only, outside locks; cannot reenter backend task APIs.
     * Caller context remains valid through successful stop. */
    esp_display_present_progress_fn on_progress;
    void *progress_user;
    bool tile_in_psram, image_in_psram, logical_in_psram;
    bool fixed_rotation; /* Runtime rotation preserves producer dimensions; physical view may transpose. */
    bool tx_staging; /* region async copies from external tiles to internal DMA conversion slots. */
    size_t transfer_capacity; /* region: 0 uses tile count. */
    bool deferred_submit; /* Engine-owned region backend worker; READY slots retain pixels. */
    esp_display_present_te_gate_t te_gate; /* Explicit injection; default retains native timing model. */
    esp_display_present_te_timing_t te_timing; /* period_us=0 retains the explicit fixed edge window. */
    size_t tile_count; /* FB PARTIAL input tiles; 0 retains the legacy default of one. */
    size_t compose_count; /* TE: 0 legacy serialized dual-image, explicit 1/2 enables pool scheduling. */
};

#ifdef __cplusplus
}
#endif
