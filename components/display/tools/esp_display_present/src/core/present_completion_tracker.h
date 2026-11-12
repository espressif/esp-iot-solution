/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "core/present_engine_types.h"
#include "backend/present_backend.h"
#include "platform/present_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Engine gate held for all access. No independent lock, task, allocation or
 * pixel ownership; terminal receipt caching does not grant buffer reuse. */
typedef struct {
    uint64_t token;
    esp_display_present_receipt_t backend;
    esp_display_present_frame_status_t status;
} present_completion_observer_t;
typedef struct {
    esp_display_present_receipt_t receipts[PRESENT_MAX_FRAMES];
    present_completion_observer_t observers[PRESENT_MAX_FRAMES];
    size_t head, count, limit;
    uint64_t committed, completed;
    esp_err_t first_error;
} present_completion_tracker_t;
void present_completion_record_error(present_completion_tracker_t *, esp_err_t);
esp_err_t present_completion_reap(present_completion_tracker_t *, const present_backend_ops_t *, present_backend_t *);
present_completion_observer_t *present_completion_find(present_completion_tracker_t *, esp_display_present_engine_receipt_t);
esp_err_t present_completion_query(present_completion_observer_t *, const present_backend_ops_t *, present_backend_t *, esp_display_present_frame_status_t *);

#ifdef __cplusplus
}
#endif
