/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once

/* Platform timer lifecycle: arm a deadline and join callbacks on destruction.
 * Used by both TE backend and GPIO synchronization; no TE window arithmetic. */
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

typedef struct present_te_timer present_te_timer_t;
typedef void (*present_te_timer_callback_t)(void *user);
esp_err_t present_te_timer_create(present_te_timer_callback_t callback, void *user, present_te_timer_t **out);
esp_err_t present_te_timer_arm(present_te_timer_t *timer, uint64_t deadline_us);
/* On failure the timer and callback context remain alive for a retry. */
esp_err_t present_te_timer_destroy(present_te_timer_t *timer, uint32_t timeout_ms);

/* Framework-owned wrapper only; excludes esp_timer and host thread stack. */
size_t present_te_timer_memory(void);
