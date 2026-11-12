/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Test panel reads source only on explicit completion, like delayed DMA. */
#pragma once
#include "backend/present_backend_types.h"
#include <assert.h>
#include <string.h>

typedef struct {
    esp_display_present_panel_event_fn callback;
    void *user;
    uint32_t width, height;
    size_t bpp, submitted;
    size_t reject_after; /* Zero disables; otherwise reject after N accepted bands. */
    bool reject, immediate, service_previous;
    struct {
        esp_display_present_surface_t surface;
        esp_display_present_rect_t area;
        uint64_t cookie;
    } pending[32];
    size_t count;
    uint8_t pixels[32 * 24 * 3];
} test_panel_t;

static inline void test_panel_complete(test_panel_t *p)
{
    assert(p->count && p->callback);
    esp_display_present_surface_t s = p->pending[0].surface;
    esp_display_present_rect_t a = p->pending[0].area;
    uint64_t cookie = p->pending[0].cookie;
    assert(a.x + a.width <= p->width && a.y + a.height <= p->height);
    for (uint32_t y = 0; y < a.height; ++y) {
        memcpy(p->pixels + ((a.y + y) * p->width + a.x) * p->bpp,
               (const uint8_t *)s.pixels + y * s.layout.stride_bytes, a.width * p->bpp);
    }
    --p->count;
    memmove(p->pending, p->pending + 1, p->count * sizeof(p->pending[0]));
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, .transaction_cookie = cookie};
    (void)p->callback(p->user, &event, false);
}

static inline esp_err_t test_panel_attach(void *context, esp_display_present_panel_event_fn callback, void *user)
{
    test_panel_t *p = context;
    p->callback = callback;
    p->user = user;
    return ESP_OK;
}

static inline esp_display_present_tx_result_t test_panel_submit(void *context, const esp_display_present_surface_t *s,
                                                                const esp_display_present_rect_t *a, uint64_t cookie)
{
    test_panel_t *p = context;
    if (p->reject || (p->reject_after && p->submitted >= p->reject_after)) {
        return (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL
        };
    }
    if (p->service_previous && p->count) {
        test_panel_complete(p);
    }
    assert(p->count < 32);
    p->pending[p->count].surface = *s;
    p->pending[p->count].area = *a;
    p->pending[p->count].cookie = cookie;
    ++p->count;
    ++p->submitted;
    if (p->immediate) {
        test_panel_complete(p);
    }
    return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
}

static inline esp_err_t test_panel_stop(void *context, uint32_t timeout)
{
    (void)timeout;
    test_panel_t *p = context;
    if (p->count) {
        return ESP_ERR_TIMEOUT;
    }
    p->callback = NULL;
    p->user = NULL;
    return ESP_OK;
}

static inline esp_display_present_panel_adapter_t test_panel_interface(test_panel_t *p)
{
    return (esp_display_present_panel_adapter_t) {
        p, test_panel_attach, test_panel_submit, test_panel_stop
    };
}
