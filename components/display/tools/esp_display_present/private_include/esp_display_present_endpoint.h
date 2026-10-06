/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_display_present.h"
#include "esp_display_present_drawbuf.h"
#include "esp_display_present_frame_tracker.h"
#include "esp_display_present_target.h"
#include "present_mode_internal.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Private two-phase-stop signal; never escapes the presenter facade. */
#define ESP_DISPLAY_PRESENT_STOP_NEEDS_CALLBACK_DETACH \
    ESP_ERR_INVALID_RESPONSE

/** Lifecycle and ISR routing only; frame data flow is owned by mode_ops. */
typedef struct {
    const char *name;
    /**
     * When true, presenter stop must be rejected while the facade still
     * holds a leased tile: the endpoint keeps no backend-held tile state of
     * its own, so the facade must return its current tile first.
     */
    bool stop_after_facade_returns_tiles;

    /**
     * Number of submitted present tickets that may remain owned by the
     * display hardware when a producer is quiescent. Scanout framebuffer
     * endpoints retain the currently displayed buffer and only report it
     * reusable after a later switch, so their steady-state tail is one.
     */
    uint8_t quiesce_present_tail;

    esp_err_t (*stop)(void *endpoint);
    /**
     * Complete a stop after target callbacks have been detached. Used only
     * when stop() returns ESP_DISPLAY_PRESENT_STOP_NEEDS_CALLBACK_DETACH for
     * a hardware-owned present tail that quiesce() explicitly permits.
     */
    esp_err_t (*finish_stop_after_callbacks)(void *endpoint);
    esp_err_t (*destroy)(void *endpoint);

    bool (*transfer_done_isr)(void *endpoint);
    bool (*frame_done_isr)(void *endpoint, BaseType_t *need_yield);

    uint64_t (*last_submitted)(const void *endpoint);
    uint64_t (*completed_transfer)(const void *endpoint);
    uint64_t (*completed_present)(const void *endpoint);
} present_endpoint_ops_t;

typedef struct {
    const present_endpoint_ops_t *ops;
    const present_mode_ops_t *mode_ops;
    void *mode_ctx;
    esp_display_present_frame_tracker_t *tracker;
    /** Owns the storage borrowed by pool-backed mode contexts. */
    esp_display_present_drawbuf_pool_t drawbuf_pool;
    esp_display_presenter_caps_t caps;
} esp_display_present_endpoint_binding_t;

esp_err_t esp_display_present_endpoint_bind(
    esp_display_present_target_t *target,
    const esp_display_presenter_config_t *config,
    esp_display_present_endpoint_binding_t *out_endpoint);

#ifdef __cplusplus
}
#endif
