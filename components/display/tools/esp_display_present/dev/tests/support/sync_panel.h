/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "test_panel.h"
#include "core/present_engine.h"
#include <pthread.h>
#include <time.h>

/* All simulated DMA and callback execution is under mutex. quiesce takes the
 * same mutex, so success establishes callback exit. Backend callbacks only
 * record events and never re-enter the adapter. */
typedef struct {
    test_panel_t panel;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    bool uncertain;
    bool worker_ready;
    esp_display_present_engine_t *presenter;
} sync_panel_t;

static inline uint64_t test_now_us(void)
{
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (uint64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

static inline void sync_panel_init(sync_panel_t *s)
{
    *s = (sync_panel_t) {
        .panel = {.width = 4, .height = 4, .bpp = 2}
    };
    assert(pthread_mutex_init(&s->mutex, NULL) == 0);
    assert(pthread_cond_init(&s->changed, NULL) == 0);
}

static inline void sync_panel_deinit(sync_panel_t *s)
{
    assert(s->panel.count == 0 && s->panel.callback == NULL);
    assert(pthread_cond_destroy(&s->changed) == 0);
    assert(pthread_mutex_destroy(&s->mutex) == 0);
}

static inline esp_err_t sync_attach(void *context, esp_display_present_panel_event_fn fn, void *user)
{
    sync_panel_t *s = context;
    pthread_mutex_lock(&s->mutex);
    esp_err_t result = test_panel_attach(&s->panel, fn, user);
    pthread_mutex_unlock(&s->mutex);
    return result;
}

static inline esp_display_present_tx_result_t sync_submit(void *context, const esp_display_present_surface_t *surface,
                                                          const esp_display_present_rect_t *area, uint64_t cookie)
{
    sync_panel_t *s = context;
    pthread_mutex_lock(&s->mutex);
    esp_display_present_tx_result_t result = test_panel_submit(&s->panel, surface, area, cookie);
    if (s->uncertain && result.acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED) {
        result = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ESP_FAIL
        };
    }
    pthread_cond_broadcast(&s->changed);
    pthread_mutex_unlock(&s->mutex);
    return result;
}

static inline esp_err_t sync_stop(void *context, uint32_t timeout)
{
    sync_panel_t *s = context;
    pthread_mutex_lock(&s->mutex);
    esp_err_t result = test_panel_stop(&s->panel, timeout);
    pthread_mutex_unlock(&s->mutex);
    return result;
}

static inline esp_display_present_panel_adapter_t sync_panel_interface(sync_panel_t *s)
{
    return (esp_display_present_panel_adapter_t) {
        s, sync_attach, sync_submit, sync_stop
    };
}

static inline void sync_complete(sync_panel_t *s)
{
    pthread_mutex_lock(&s->mutex);
    test_panel_complete(&s->panel);
    pthread_mutex_unlock(&s->mutex);
}
/* One synthetic edge followed by delayed DMA completion. The condition variable
 * synchronizes with panel.submit; no arbitrary sleep determines ordering. */
static inline void *sync_edge_and_complete(void *context)
{
    sync_panel_t *s = context;
    esp_display_present_engine_notify_te_isr(s->presenter, test_now_us());
    pthread_mutex_lock(&s->mutex);
    s->worker_ready = true;
    pthread_cond_broadcast(&s->changed);
    while (!s->panel.count) {
        pthread_cond_wait(&s->changed, &s->mutex);
    }
    test_panel_complete(&s->panel);
    pthread_mutex_unlock(&s->mutex);
    return NULL;
}

static inline pthread_t sync_start_worker(sync_panel_t *s, esp_display_present_engine_t *presenter)
{
    s->presenter = presenter;
    s->worker_ready = false;
    pthread_t thread;
    assert(pthread_create(&thread, NULL, sync_edge_and_complete, s) == 0);
    pthread_mutex_lock(&s->mutex);
    while (!s->worker_ready) {
        pthread_cond_wait(&s->changed, &s->mutex);
    }
    pthread_mutex_unlock(&s->mutex);
    return thread;
}
