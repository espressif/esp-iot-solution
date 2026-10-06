/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Renderer-facing public entry point: the presenter draw contract and
 * lifecycle API. Low-level surface/partition APIs require explicit includes
 * and are implementation details for most integrations. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_display_present_config.h"
#include "esp_display_present_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default per-push panel transfer completion budget. */
#define ESP_DISPLAY_PRESENT_DEFAULT_TRANSFER_TIMEOUT_MS 1000U

/** Default FB pipeline free-buffer acquire budget. */
#define ESP_DISPLAY_PRESENT_DEFAULT_PIPELINE_ACQUIRE_TIMEOUT_MS 1000U

/**
 * Query the minimum number of panel framebuffers before creating the panel.
 *
 * Set an explicit RGB, MIPI-DSI or IO panel type. Only panel type, mode,
 * rotation and TE configuration are read; handles, supplied framebuffers,
 * pixel format and draw-buffer settings are ignored. This query performs
 * static policy validation without accessing hardware or allocating memory.
 *
 * IO/GRAM returns 0: draw buffers and TE compose buffers are separate.
 * Presenter creation still validates the actual handles and buffer storage.
 * @p out_count is set to 0 on error when non-NULL.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for invalid arguments,
 *         ESP_ERR_NOT_SUPPORTED for an unresolved panel type or unsupported
 *         mode, rotation or TE combination.
 */
esp_err_t esp_display_present_get_required_frame_buffer_count(
    const esp_display_present_target_config_t *config,
    uint8_t *out_count);

/**
 * How GSP should talk to this presenter.
 *
 * This is the public draw contract, not the transport implementation.
 * Transport and framebuffer policy stay inside the presenter.
 */
typedef enum {
    /** Submit partitioned areas through a shared draw buffer. */
    ESP_DISPLAY_PRESENT_CONTRACT_PARTITION = 0,
    /** Submit dirty regions into a coherent draw surface. */
    ESP_DISPLAY_PRESENT_CONTRACT_DIRECT,
    /** Submit a full frame. */
    ESP_DISPLAY_PRESENT_CONTRACT_FULL,
} esp_display_present_contract_t;

/** Explicit coverage of a DIRECT/FULL surface submission. */
typedef enum {
    /** The complete logical screen has been produced. */
    ESP_DISPLAY_PRESENT_COVERAGE_FULL = 0,
    /** Only listed logical areas have been produced. */
    ESP_DISPLAY_PRESENT_COVERAGE_AREAS,
} esp_display_present_coverage_t;

/** Pixel surface description. */
typedef struct {
    void *pixels; /**< Address of the first pixel. */
    size_t stride_bytes; /**< Distance between adjacent rows in bytes. */
    uint16_t width; /**< Surface width in pixels. */
    uint16_t height; /**< Surface height in pixels. */
    esp_display_present_pixel_format_t pixel_format; /**< Pixel storage format. */
} esp_display_presenter_surface_t;

/** Presenter creation parameters. */
typedef struct {
    uint16_t width;  /**< Logical display width in pixels. */
    uint16_t height; /**< Logical display height in pixels. */
    /** Renderer output format; must match target.hw.input_pixel_format. */
    esp_display_present_pixel_format_t pixel_format;
    size_t max_damage_areas; /**< Maximum dirty areas accepted per frame. */
    /**
     * Per-push panel transfer completion budget in milliseconds; 0 selects
     * ESP_DISPLAY_PRESENT_DEFAULT_TRANSFER_TIMEOUT_MS. A timeout faults the
     * presenter with ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT, so raise
     * this for slow panels instead of accepting a terminal fault.
     */
    uint32_t transfer_timeout_ms;
    /**
     * FB pipeline free-buffer acquire budget in milliseconds; 0 selects
     * ESP_DISPLAY_PRESENT_DEFAULT_PIPELINE_ACQUIRE_TIMEOUT_MS.
     */
    uint32_t pipeline_acquire_timeout_ms;
    /** Panel hardware, present mode and endpoint memory configuration. */
    esp_display_present_target_config_t target;
} esp_display_presenter_config_t;

/** Renderer-visible capabilities of a presenter. */
typedef struct {
    /** Public draw contract seen by GSP. */
    esp_display_present_contract_t contract;
    /** DIRECT/FULL only; true when submit may use COVERAGE_AREAS. */
    bool supports_coverage_areas;
    /** DIRECT/FULL only: pixels remain valid across frames for dirty rendering. */
    bool surface_retains_content;
    /** DIRECT/FULL only: previous displayed surface can be read for scroll blits. */
    bool previous_surface_readable;
    uint16_t width;  /**< Logical display width in pixels. */
    uint16_t height; /**< Logical display height in pixels. */
    size_t stride_bytes; /**< Preferred full-surface row stride in bytes. */
    esp_display_present_pixel_format_t pixel_format; /**< Renderer output format. */
    /** PARTITION only: bytes available in one acquired draw region. */
    size_t drawbuf_bytes;
    size_t max_damage_areas; /**< Maximum dirty areas accepted per frame. */
} esp_display_presenter_caps_t;

/** Coverage supplied when submitting a DIRECT or FULL surface. */
typedef struct {
    /**
     * Explicit coverage for a submitted surface. No-op updates are handled
     * before acquire/submit. FULL ignores areas. AREAS requires non-empty areas.
     */
    esp_display_present_coverage_t coverage;
    const esp_display_present_area_t *areas; /**< Covered logical areas for AREAS coverage. */
    size_t area_count; /**< Number of entries in @c areas. */
} esp_display_presenter_submit_t;

/** @brief Drawable logical region leased for the current frame. */
typedef struct {
    esp_display_presenter_surface_t surface; /**< Region pixel storage. */
    uint16_t origin_x; /**< Logical X origin of the region. */
    uint16_t origin_y; /**< Logical Y origin of the region. */
} esp_display_presenter_region_t;

/**
 * @brief Producer-neutral drawable buffer lease.
 *
 * PARTITION/TE leases expose reusable scratch storage; FULL leases expose the
 * complete logical surface. The producer chooses the logical area and the
 * packed stride when submitting. @p capacity_bytes is the authoritative
 * storage limit; surface dimensions describe the presenter's preferred
 * full-width layout.
 */
typedef esp_err_t (*esp_display_presenter_resolve_rows_cb_t)(
    void *user_ctx, uint32_t lease_id, size_t stride_bytes,
    size_t requested_rows, size_t *out_rows);

/** Producer-owned drawable-buffer lease. */
typedef struct {
    esp_display_presenter_surface_t surface; /**< Preferred drawable surface layout. */
    size_t capacity_bytes; /**< Authoritative writable storage capacity. */
    uint32_t lease_id; /**< Identifier required by resolve, submit, or cancel. */
    /** Resolve a legal row count after the producer chooses its stride. */
    esp_display_presenter_resolve_rows_cb_t resolve_rows;
    void *resolve_rows_ctx; /**< Opaque context for @c resolve_rows. */
} esp_display_presenter_buffer_t;

typedef struct esp_display_presenter esp_display_presenter_t;

typedef enum {
    ESP_DISPLAY_PRESENTER_STATE_OPEN = 0,
    ESP_DISPLAY_PRESENTER_STATE_CLOSING,
    ESP_DISPLAY_PRESENTER_STATE_STOPPED,
    ESP_DISPLAY_PRESENTER_STATE_FAULTED,
} esp_display_presenter_state_t;

/**
 * Create a renderer endpoint and bind all required target callbacks.
 *
 * Renderer calls are single-task and serialize acquire, submit and
 * cancel. A control task may request stop, but delete has one exclusive owner
 * and must not race another API call. Hardware callbacks run in ISR context;
 * the component keeps their state alive until stop has drained them.
 */
esp_err_t esp_display_presenter_create(
    const esp_display_presenter_config_t *config,
    esp_display_presenter_t **out_presenter);

/**
 * Enter CLOSING and stop after all asynchronous work has retired. New acquire
 * calls fail with ESP_ERR_INVALID_STATE. An active lease must be cancelled by
 * its renderer. Retry ESP_ERR_INVALID_STATE while renderer calls or hardware
 * work are in flight. A terminal hardware fault never reopens the presenter.
 */
esp_err_t esp_display_presenter_stop(
    esp_display_presenter_t *presenter);

/**
 * Stop, detach callbacks and release the target and endpoint.
 *
 * On ESP_ERR_INVALID_STATE the object remains owned by the caller and must be
 * retained for a later retry. Do not call delete concurrently from two tasks.
 */
esp_err_t esp_display_presenter_delete(
    esp_display_presenter_t *presenter);

/**
 * Reconfigure logical-to-physical rotation without changing panel scan mode.
 *
 * The producer must first stop submitting and quiesce the presenter. The
 * presenter object remains valid and keeps the same address; its target and
 * endpoint are rebuilt so all rotation-dependent storage and transforms are
 * updated consistently. On failure, the previous configuration is restored.
 */
esp_err_t esp_display_presenter_set_rotation(
    esp_display_presenter_t *presenter,
    esp_display_present_rotation_t rotation);

esp_err_t esp_display_presenter_get_caps(
    const esp_display_presenter_t *presenter,
    esp_display_presenter_caps_t *out_caps);

/**
 * PARTITION only: borrow the presenter's drawbuf pool pointers.
 *
 * The pointers remain owned by the presenter for its lifetime. Producers such
 * as LVGL may register them with lv_display_set_buffers() and render directly
 * into them so flush can submit without an extra memcpy hop.
 */
typedef struct {
    void *buffers[2]; /**< Borrowed draw-buffer addresses. */
    uint8_t count; /**< Number of valid draw-buffer addresses. */
    uint16_t lines; /**< Preferred draw-buffer height in rows. */
    size_t bytes; /**< Capacity of each draw buffer in bytes. */
} esp_display_presenter_drawbuf_t;

esp_err_t esp_display_presenter_get_drawbuf(
    const esp_display_presenter_t *presenter,
    esp_display_presenter_drawbuf_t *out_drawbuf);

/**
 * Rebind the serial render-task owner after a platform session handoff.
 * Valid only between frames; it retains pending hardware completion fences.
 */
esp_err_t esp_display_presenter_rebind_producer(
    esp_display_presenter_t *presenter);

/** Read the monotonic lifecycle state; FAULTED never reopens in place. */
esp_err_t esp_display_presenter_get_state(
    const esp_display_presenter_t *presenter,
    esp_display_presenter_state_t *out_state);

/**
 * Read why the presenter faulted. ESP_DISPLAY_PRESENT_FAULT_NONE means no
 * fault was recorded. A transfer timeout is recoverable by deleting and
 * recreating the presenter only when the endpoint can prove that hardware no
 * longer owns its buffers (for example the framebuffer endpoint's fault-stop
 * path). GRAM/TE endpoints must retain storage until their transfer callback
 * arrives. A producer protocol fault is a producer bug.
 */
esp_err_t esp_display_presenter_get_fault_reason(
    const esp_display_presenter_t *presenter,
    esp_display_present_fault_reason_t *out_reason);

/** Map physical input coordinates into the logical render orientation. */
esp_err_t esp_display_presenter_map_point(
    const esp_display_presenter_t *presenter,
    int32_t *x,
    int32_t *y);

/**
 * Begin a frame on the presenter-owned timeline and return the logical areas
 * requiring rasterization. The asynchronous completion ticket is private to
 * the presenter.
 */
esp_err_t esp_display_presenter_begin_next_frame(
    esp_display_presenter_t *presenter,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count,
    bool *out_full_coverage);

/** @brief Lease producer storage without choosing a logical damage area. */
esp_err_t esp_display_presenter_acquire_buffer(
    esp_display_presenter_t *presenter,
    esp_display_presenter_buffer_t *out_buffer);

/**
 * @brief Submit the rendered portion of a buffer lease.
 *
 * Pixels start at buffer.surface.pixels and use @p stride_bytes. The inclusive
 * logical @p area supplies the destination position and rendered dimensions.
 */
esp_err_t esp_display_presenter_submit_buffer(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area,
    size_t stride_bytes);

/** @brief Commit the active frame with explicit logical coverage. */
esp_err_t esp_display_presenter_commit_frame(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_submit_t *submit);

/** @brief Cancel the active pre-commit frame and any held region from its
 *  bound producer task. */
void esp_display_presenter_cancel_frame(
    esp_display_presenter_t *presenter);

/**
 * Wait until all work accepted before this call has completed transfer and
 * presentation. The producer must already be stopped between frames.
 */
esp_err_t esp_display_presenter_quiesce(
    esp_display_presenter_t *presenter,
    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
