/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "pixel/present_pixel_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Engine-only fast path. Submit validated this unchanged request before
 * queueing; caller keeps both surfaces alive and serializes overlapping writes.
 * Executes synchronously without validation, allocation, tickets or callbacks. */
void present_pixel_copy_cpu_validated(const esp_display_present_copy_request_t *request);

#ifdef __cplusplus
}
#endif
