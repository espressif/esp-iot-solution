/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "core/present_engine_types.h"
#include "backend/present_backend_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Owner-neutral facade control. These declarations do not grant producer
 * access to frames or leases. Stop/destroy require the producer to be paused;
 * status/receipt operations retain the engine's existing serialization. */

/* Session controller only: caller excludes all producer calls (including native
 * calls while drain releases the engine gate). Drain before committing owner;
 * failure preserves owner and never cancels accepted transfers. */
esp_err_t present_presenter_control_bind_producer(esp_display_present_engine_t *, uint32_t);
esp_err_t present_presenter_control_get_status(esp_display_present_engine_t *, esp_display_present_engine_status_t *);
esp_err_t present_presenter_control_get_status_snapshot(esp_display_present_engine_t *, esp_display_present_engine_status_t *, bool *valid);
esp_err_t present_presenter_control_query_frame(esp_display_present_engine_t *, esp_display_present_engine_receipt_t, esp_display_present_frame_status_t *);
esp_err_t present_presenter_control_release(esp_display_present_engine_t *, esp_display_present_engine_receipt_t);
esp_err_t present_presenter_control_stop(esp_display_present_engine_t *, uint32_t);
esp_err_t present_presenter_control_destroy(esp_display_present_engine_t *);
esp_err_t present_presenter_control_cancel_tracking(esp_display_present_engine_t *, esp_display_present_engine_receipt_t);
esp_err_t present_presenter_control_finish_cancel(esp_display_present_engine_t *, uint32_t);

#ifdef PRESENT_HOST
void present_presenter_test_gate_lock(esp_display_present_engine_t *);
void present_presenter_test_gate_unlock(esp_display_present_engine_t *);
#endif

#ifdef __cplusplus
}
#endif
