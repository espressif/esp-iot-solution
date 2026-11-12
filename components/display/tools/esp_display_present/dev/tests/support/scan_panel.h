/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_types.h"
#include <assert.h>
#include <string.h>

/* Continuous scanout model. A confirmation means replacement is active AND
 * the previous source is retired. No IO transfer-done implies retirement. */
typedef struct {
    esp_display_present_panel_event_fn callback;
    void *user;
    esp_display_present_surface_t current;
    esp_display_present_surface_t pending;
    uint64_t cookie;
    size_t submissions;
    bool immediate;
    bool reject;
    bool uncertain;
    bool early_reject;
    bool stop_timeout;
    uint8_t pixels[4096];
} scan_panel_t;
static inline void scan_confirm(scan_panel_t *p)
{
    assert(p->cookie && p->callback);
    uint64_t cookie = p->cookie;
    p->current = p->pending;
    assert(p->current.capacity_bytes <= sizeof(p->pixels));
    memcpy(p->pixels, p->current.pixels, p->current.capacity_bytes);
    p->pending = (esp_display_present_surface_t) {
        0
    };
    p->cookie = 0;
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED, .transaction_cookie = cookie};
    (void)p->callback(p->user, &event, false);
}

static inline esp_err_t scan_attach(void *context, esp_display_present_panel_event_fn callback, void *user)
{
    scan_panel_t *p = context;
    p->callback = callback;
    p->user = user;
    return ESP_OK;
}

static inline esp_display_present_tx_result_t scan_submit(void *context, const esp_display_present_surface_t *source,
                                                          const esp_display_present_rect_t *area, uint64_t cookie)
{
    scan_panel_t *p = context;
    if (p->reject) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL
        };
    }
    assert(!p->cookie && !area->x && !area->y);
    assert(area->width == source->layout.width && area->height == source->layout.height);
    p->pending = *source;
    p->cookie = cookie;
    p->submissions++;
    if (p->immediate) {
        scan_confirm(p);
    }
    if (p->uncertain) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ESP_FAIL
        };
    }
    if (p->early_reject) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL
        };
    }
    return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
}

static inline esp_err_t scan_stop(void *context, uint32_t timeout)
{
    (void)timeout;
    scan_panel_t *p = context;
    if (p->stop_timeout) {
        return ESP_ERR_TIMEOUT;
    }
    p->current = (esp_display_present_surface_t) {
        0
    };
    p->pending = (esp_display_present_surface_t) {
        0
    };
    p->cookie = 0;
    p->callback = NULL;
    return ESP_OK; /* Simulated scan engine off, including pending switch. */
}

static inline esp_display_present_panel_adapter_t scan_interface(scan_panel_t *p)
{
    return (esp_display_present_panel_adapter_t) {
        p, scan_attach, scan_submit, scan_stop
    };
}
