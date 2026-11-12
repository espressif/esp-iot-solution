/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_display_present_engine esp_display_present_engine_t;
typedef struct {
    int32_t x1, y1, x2, y2;
} esp_display_present_engine_area_t; /* Inclusive, v1-style. */
typedef enum {
    ESP_DISPLAY_PRESENT_ENGINE_RUNNING,
    ESP_DISPLAY_PRESENT_ENGINE_CLOSING,
    ESP_DISPLAY_PRESENT_ENGINE_STOPPED,
} esp_display_present_engine_state_t;
typedef struct {
    esp_display_present_engine_state_t state;
    bool frame_active;
    bool cancel_pending;
    size_t pending_frames;
    uint64_t committed_frames;
    uint64_t completed_frames; /* Terminal transfers, not necessarily successful. */
    esp_err_t first_error; /* Sticky accepted-operation error, not a transient rejection. */
    esp_err_t last_service_error; /* Latest pump result, including retryable rejection. */
} esp_display_present_engine_status_t;

/** Optional, bounded observer records (32 per presenter), independent of backend
 * buffer ownership. Tokens are instance-scoped; callers explicitly release a
 * terminal observer or cancel tracking. Untracked commit remains unchanged. */
typedef struct {
    uint64_t value;
} esp_display_present_engine_receipt_t;

#ifdef __cplusplus
}
#endif
