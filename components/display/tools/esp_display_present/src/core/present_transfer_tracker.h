/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_types.h"
#include "platform/present_limits.h"
#include "platform/present_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Timing evidence only; never owns pixels or manufactures completion.
 * The engine supplies its original control lock. Backend callbacks and worker
 * signals must run after the helpers release it. */
typedef struct {
    uint64_t cookie, started_us;
    bool reported;
} present_transfer_entry_t;
typedef struct {
    uint64_t timeout_us; /* Immutable before panel callback registration. */
    present_transfer_entry_t *entries;
} present_transfer_tracker_t;
esp_err_t present_transfer_init(present_transfer_tracker_t *, uint32_t timeout_ms);
void present_transfer_deinit(present_transfer_tracker_t *);
size_t present_transfer_memory(uint32_t timeout_ms);
/* begin/expired/rejected require an enabled, successfully initialized policy. */
size_t present_transfer_begin(present_transfer_tracker_t *, present_lock_t *, uint64_t cookie);
void present_transfer_rejected(present_transfer_tracker_t *, present_lock_t *, size_t index, uint64_t cookie);
void present_transfer_quiesced(present_transfer_tracker_t *, present_lock_t *);
bool present_transfer_finish(present_transfer_tracker_t *, present_lock_t *,
                             const esp_display_present_backend_event_t *, bool isr, uint64_t *timestamp_us);
/* Task-only; engine gate held so no submission starts during collection. */
size_t present_transfer_expired(present_transfer_tracker_t *, present_lock_t *, uint64_t now, uint64_t *cookies);
/* May run outside gate; stamp insertion is serialized by control instead. */
uint32_t present_transfer_wait_ms(present_transfer_tracker_t *, present_lock_t *);

#ifdef __cplusplus
}
#endif
