/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_types.h"
#include "backend/present_records.h"

/* One authority for frame records. All access, including ISR completion, must
 * hold the owning backend lock. No allocation, hardware or callbacks while
 * progressing records. Pending counts accepted/reserved accesses, not images.
 * Output policies still supply presentation evidence; terminal is not present.
 */
typedef enum {
    PRESENT_CORE_RUNNING,
    PRESENT_CORE_CLOSING,
    PRESENT_CORE_QUIESCENT,
} present_core_life_t;
typedef struct {
    present_record_t common;
    size_t pending, leased;
    bool sealed, canceled;
} present_core_frame_t;
typedef struct {
    present_core_frame_t *frames;
    size_t count, active;
    present_cancel_history_t canceled;
    esp_display_present_backend_fault_t fault;
    present_core_life_t life;
} present_frame_core_t;
esp_err_t present_frame_core_init(present_frame_core_t *core, size_t count);
void present_frame_core_deinit(present_frame_core_t *core);
present_core_frame_t *present_frame_core_find(present_frame_core_t *core, uint64_t token);
present_core_frame_t *present_frame_core_find_receipt(present_frame_core_t *core, esp_display_present_receipt_t receipt);
esp_err_t present_frame_core_admit(present_frame_core_t *core, present_lock_t *lock,
                                   uint32_t width, uint32_t height,
                                   const esp_display_present_frame_request_t *request, esp_display_present_frame_t *out);
esp_err_t present_frame_core_begin(present_frame_core_t *core, esp_display_present_frame_t *out);
esp_err_t present_frame_core_seal(present_frame_core_t *core, present_core_frame_t *frame, esp_display_present_receipt_t *out);
void present_frame_core_reap(present_frame_core_t *core, present_core_frame_t *frame);
void present_frame_core_complete(present_frame_core_t *core, present_core_frame_t *frame);
void present_frame_core_fail(present_frame_core_t *core, present_core_frame_t *frame, uint64_t cookie, esp_err_t error);
esp_err_t present_frame_core_release(present_core_frame_t *frame);
esp_err_t present_frame_core_query(present_frame_core_t *core, present_lock_t *lock, esp_display_present_receipt_t receipt,
                                   bool supports_present, esp_display_present_frame_status_t *out);
esp_err_t present_frame_core_get_fault(present_frame_core_t *core, present_lock_t *lock, esp_display_present_backend_fault_t *out);
esp_err_t present_frame_core_wait(present_backend_t *backend,
                                  esp_err_t (*query)(present_backend_t *, esp_display_present_receipt_t, esp_display_present_frame_status_t *),
                                  esp_display_present_receipt_t receipt, esp_display_present_completion_point_t point,
                                  bool supports_present, uint32_t timeout, esp_display_present_frame_status_t *out);
esp_err_t present_frame_core_release_receipt(present_frame_core_t *core, present_lock_t *lock, esp_display_present_receipt_t receipt);
esp_err_t present_frame_core_can_destroy(const present_frame_core_t *core);
/* Call only after output/copy quiescence. Rejects outstanding access/leases. */
esp_err_t present_frame_core_quiescent(present_frame_core_t *core);

/* Complete-image policies establish drain explicitly (FB confirmation ends
 * frame receipt ownership, not the separate scanout allocation reference). */
void present_frame_core_terminal(present_frame_core_t *, present_core_frame_t *);
