/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TE compose buffer pool. GRAM DMA reads the submitted surface, not the
 * previous baseline, so ownership is RETIRE_ON_SUBMIT:
 *   commit(A, B): A → free immediately, B stays inflight (DMA)
 *   DMA-done ISR: B SUBMITTED → BASELINE (B is not freed)
 * Producer can repair/compose A while DMA reads B. The LCD bus still
 * serializes transfers: the next commit waits for B's ticket first.
 * This pool is not the tracker pool.
 */
typedef struct present_te_buf {
    void *buffer;
    void *new_display_buffer;
    uint64_t ticket;
    STAILQ_ENTRY(present_te_buf) entry;
} present_te_buf_t;

typedef struct {
    portMUX_TYPE lock;
    STAILQ_HEAD(present_te_inflight_list, present_te_buf) inflight_queue;
    STAILQ_HEAD(present_te_free_list, present_te_buf) free_queue;
    SemaphoreHandle_t avail_sem;
    present_te_buf_t *elems;
    uint8_t elem_count;
    uint32_t acquire_timeout_ms;
} present_te_pool_t;

esp_err_t present_te_pool_init(
    present_te_pool_t *pool,
    void **buffers,
    uint8_t buffer_count,
    uint32_t acquire_timeout_ms);

void present_te_pool_deinit(present_te_pool_t *pool);

bool present_te_pool_enabled(const present_te_pool_t *pool);

/** Free old display A now; keep submitted B inflight until DMA done. */
bool present_te_pool_commit(
    present_te_pool_t *pool,
    void *old_display,
    void *new_display,
    uint64_t ticket);

bool present_te_pool_cancel_commit(
    present_te_pool_t *pool,
    void *old_display,
    void *new_display);

esp_err_t present_te_pool_acquire_next(
    present_te_pool_t *pool,
    const bool *closing,
    void **out_buffer);

bool present_te_pool_retire_isr(
    present_te_pool_t *pool,
    void **out_new_display,
    uint64_t *out_ticket);

bool present_te_pool_has_inflight(const present_te_pool_t *pool);

#ifdef __cplusplus
}
#endif
