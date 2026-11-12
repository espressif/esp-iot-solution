/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_types.h"
#include "platform/present_os.h"
#include "platform/present_limits.h"

/* Private value/record policy only. Caller owns locking, completion timing,
 * pool storage and recycling. No hardware, callbacks or buffer ownership here.
 * Embed first in backend-specific records when using present_record_find. */
typedef struct {
    uint64_t token;
    bool external;
    bool terminal;
    esp_err_t result;
} present_record_t;

static inline bool present_valid_rect(uint32_t width, uint32_t height, const esp_display_present_rect_t *area)
{
    return area && area->width && area->height && area->x < width && area->y < height &&
           area->width <= width - area->x && area->height <= height - area->y;
}
static inline esp_err_t present_validate_damage(uint32_t width, uint32_t height,
                                                const esp_display_present_rect_t *areas, size_t count)
{
    if (count > PRESENT_MAX_DAMAGE_AREAS || (count && !areas)) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!present_valid_rect(width, height, &areas[i])) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}
static inline uint32_t present_remaining_ms(uint64_t start, uint32_t timeout)
{
    uint64_t elapsed = present_now_ms() - start;
    return timeout == UINT32_MAX ? UINT32_MAX : elapsed >= timeout ? 0 : (uint32_t)(timeout - elapsed);
}
static inline void *present_record_find(void *records, size_t count, size_t stride,
                                        uint64_t token, bool external)
{
    if (!token) {
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        present_record_t *record = (present_record_t *)((uint8_t *)records + i * stride);
        if (record->token == token && (!external || record->external)) {
            return record;
        }
    }
    return NULL;
}
static inline void PRESENT_ISR_ATTR present_record_fail(present_record_t *record, esp_err_t error)
{
    if (record && record->result == ESP_OK && error != ESP_OK) {
        record->result = error;
    }
}
static inline void PRESENT_ISR_ATTR present_fault_first(esp_display_present_backend_fault_t *fault,
                                                        uint64_t token, uint64_t cookie, esp_err_t error)
{
    if (error != ESP_OK && fault->result == ESP_OK) {
        fault->result = error;
        fault->frame.value = token;
        fault->transaction_cookie = cookie;
    }
}
/* Evidence is supplied by the backend: terminal != successful presentation. */
static inline esp_display_present_frame_status_t present_record_status(const present_record_t *record,
                                                                       bool sealed, bool released, bool terminal, bool presented)
{
    return (esp_display_present_frame_status_t) {
        .sealed = sealed,
        .src_released = released,
        .terminal = terminal,
        .present_confirmed = presented,
        .result = record->result
    };
}

/* One-entry idempotence history, not an unbounded cancellation registry.
 * The backend publishes only after all accesses for that frame have ended. */
typedef struct {
    uint64_t token;
} present_cancel_history_t;
static inline bool present_cancel_matches(const present_cancel_history_t *history, uint64_t token)
{
    return token && history->token == token;
}
static inline void PRESENT_ISR_ATTR present_cancel_publish(present_cancel_history_t *history, uint64_t token)
{
    history->token = token;
}
