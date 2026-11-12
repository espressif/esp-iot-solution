/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "pixel/present_pixel_engine.h"

/* Internal composition core, not a panel backend. All calls are serialized
 * task-context calls. TE owns leases, receipts and panel event correlation.
 * write() drains its borrow; queue_write() retains it until finish_write(). */
typedef struct present_te_composer present_te_composer_t;
typedef struct {
    uint64_t value;
} present_te_frame_t;
typedef struct {
    uint32_t width, height;
    bool image_in_psram;
    size_t image_count; /* 0 -> 2; explicit 1 or 2. */
    esp_display_present_engine_pixel_format_t format;
    esp_display_present_byte_order_t panel_order;
    esp_display_present_engine_rotation_t rotation; /* Applied to producer writes only, not repair. */
    esp_display_present_pixel_config_t pixel; /* max_jobs=0 selects synchronous default. */
} present_te_composer_config_t;

esp_err_t present_te_composer_create(const present_te_composer_config_t *, present_te_composer_t **);
/* One construction writer, with up to two READY/INFLIGHT image slots. Explicit
 * image_count publishes the content baseline at READY; legacy zero serializes
 * frames and publishes only on successful transfer. Tokens reject stale reuse. */
esp_err_t present_te_composer_begin(present_te_composer_t *, present_te_frame_t *);
esp_err_t present_te_composer_write(present_te_composer_t *, present_te_frame_t,
                                    const esp_display_present_source_t *, const esp_display_present_rect_t *src_area,
                                    uint32_t dst_x, uint32_t dst_y);
/* Tracks successful writes. An incomplete first frame stays BUILDING;
 * asynchronous repair returns TIMEOUT and service retries until READY. */
esp_err_t present_te_composer_seal(present_te_composer_t *, present_te_frame_t);
/* Only a DEFINITELY unaccepted transmission can return to READY. Retry waits
 * for a newer edge. ACCEPTED/UNCERTAIN must stay pinned until access ends. */
esp_err_t present_te_composer_reject(present_te_composer_t *, present_te_frame_t);
/* Call only after all peripheral reads end, including safe abort. Legacy zero
 * image_count publishes on success; explicit pools already published at READY.
 * On failure the backend must fault queued successors; content version does not
 * imply successful presentation. No display confirmation claim. */
esp_err_t present_te_composer_complete(present_te_composer_t *, present_te_frame_t, esp_err_t result);
/* BUILDING/REPAIR/READY only; pending repair can return TIMEOUT; INFLIGHT cannot be canceled or destroyed. */
esp_err_t present_te_composer_cancel(present_te_composer_t *, present_te_frame_t);
esp_err_t present_te_composer_destroy(present_te_composer_t *);

/* Queued writes retain source until finish_write succeeds (operation result is
 * separate). Seal/cancel refuse while tickets remain. All composer calls are
 * serialized; pixel notifications must only wake the owner, not call back here. */
esp_err_t present_te_composer_queue_write(present_te_composer_t *, present_te_frame_t,
                                          const esp_display_present_source_t *, const esp_display_present_rect_t *, uint32_t, uint32_t, esp_display_present_copy_ticket_t *);
esp_err_t present_te_composer_finish_write(present_te_composer_t *, esp_display_present_copy_ticket_t, esp_err_t *);
esp_err_t present_te_composer_stop(present_te_composer_t *, uint32_t);

esp_err_t present_te_composer_set_rotation(present_te_composer_t *, esp_display_present_engine_rotation_t);

/* Output policy has already established admission; no timing model here. */
esp_err_t present_te_composer_pin(present_te_composer_t *, present_te_frame_t, esp_display_present_source_t *);

/* Drained topology-preserving reinterpretation, same pixel count. */
esp_err_t present_te_composer_reorient(present_te_composer_t *, esp_display_present_engine_rotation_t, uint32_t, uint32_t);
