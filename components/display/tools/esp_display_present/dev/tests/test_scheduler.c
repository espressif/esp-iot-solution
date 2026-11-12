/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "../../src/platform/present_os.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define OK(x) assert((x) == ESP_OK)

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t condition;
    esp_display_present_panel_event_fn callback;
    void *user;
    esp_display_present_surface_t surface;
    uint64_t cookie;
    unsigned submissions;
    unsigned quiesces;
    bool blocked;
    bool release;
    bool reject;
    bool immediate;
    bool uncertain;
    bool fb;
} panel_t;
static void complete_locked(panel_t *p)
{
    assert(p->callback && p->cookie);
    uint16_t value;
    memcpy(&value, p->surface.pixels, sizeof(value));
    assert(value == 0xabab);
    esp_display_present_backend_event_t event = {.kind = p->fb ? ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED : ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE,
                                                 .transaction_cookie = p->cookie
                                                };
    p->cookie = 0;
    (void)p->callback(p->user, &event, false);
}

static esp_err_t attach(void *context, esp_display_present_panel_event_fn callback, void *user)
{
    panel_t *p = context;
    p->callback = callback;
    p->user = user;
    return ESP_OK;
}

static esp_display_present_tx_result_t submit(void *context, const esp_display_present_surface_t *surface, const esp_display_present_rect_t *area, uint64_t cookie)
{
    (void)area;
    panel_t *p = context;
    pthread_mutex_lock(&p->lock);
    assert(!p->cookie);
    p->surface = *surface;
    p->submissions++;
    pthread_cond_broadcast(&p->condition);
    while (p->blocked && !p->release) {
        pthread_cond_wait(&p->condition, &p->lock);
    }
    bool reject = p->reject;
    bool uncertain = p->uncertain;
    if (!reject) {
        p->cookie = cookie;
        if (p->immediate) {
            complete_locked(p);
        }
    }
    pthread_mutex_unlock(&p->lock);
    return (esp_display_present_tx_result_t) {
        reject ? ESP_DISPLAY_PRESENT_TX_REJECTED : uncertain ? ESP_DISPLAY_PRESENT_TX_UNCERTAIN : ESP_DISPLAY_PRESENT_TX_ACCEPTED,
               reject || uncertain ? ESP_FAIL : ESP_OK
    };
}

static esp_err_t quiesce(void *context, uint32_t timeout)
{
    (void)timeout;
    panel_t *p = context;
    pthread_mutex_lock(&p->lock);
    p->quiesces++;
    esp_err_t result = p->cookie ? ESP_ERR_TIMEOUT : ESP_OK;
    if (result == ESP_OK) {
        p->callback = NULL;
    }
    pthread_mutex_unlock(&p->lock);
    return result;
}

static void init_panel(panel_t *p)
{
    *p = (panel_t) {
        0
    };
    assert(pthread_mutex_init(&p->lock, NULL) == 0);
    pthread_condattr_t attr;
    assert(pthread_condattr_init(&attr) == 0);
    assert(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0);
    assert(pthread_cond_init(&p->condition, &attr) == 0);
    pthread_condattr_destroy(&attr);
}

static void wait_submit(panel_t *p, unsigned count)
{
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += 2;
    pthread_mutex_lock(&p->lock);
    while (p->submissions < count) {
        assert(pthread_cond_timedwait(&p->condition, &p->lock, &deadline) == 0);
    }
    pthread_mutex_unlock(&p->lock);
}

static esp_display_present_engine_t *create(panel_t *panel, bool fb)
{
    panel->fb = fb;
    esp_display_present_engine_config_t config = {.backend = {
            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = fb ? 3 : 2, .max_frames = 4,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {panel, attach, submit, quiesce},
            .te_window_us = 1000000, .te_budget_us = 1000
        },
        .backend_kind = fb ? ESP_DISPLAY_PRESENT_ENGINE_FB : ESP_DISPLAY_PRESENT_ENGINE_TE,
        .timeout_ms = 1000, .background_service = true
    };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&config, &p));
    return p;
}

static void frame(esp_display_present_engine_t *p)
{
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    for (int y = 0; y < 4; y += 2) {
        esp_display_present_lease_t lease;
        OK(esp_display_present_engine_acquire_buffer(p, &lease));
        memset(lease.surface.pixels, 0xab, 16);
        esp_display_present_engine_area_t area = {0, y, 3, y + 1};
        OK(esp_display_present_engine_submit_buffer(p, &lease, &area, 8));
    }
    OK(esp_display_present_engine_commit_frame(p, NULL));
}

static void complete(panel_t *p)
{
    pthread_mutex_lock(&p->lock);
    complete_locked(p);
    pthread_mutex_unlock(&p->lock);
}

static void destroy(esp_display_present_engine_t *presenter, panel_t *panel)
{
    OK(esp_display_present_engine_stop(presenter, 1000));
    OK(esp_display_present_engine_destroy(presenter));
    pthread_cond_destroy(&panel->condition);
    pthread_mutex_destroy(&panel->lock);
}

static void autonomous(bool fb)
{
    panel_t panel;
    init_panel(&panel);
    esp_display_present_engine_t *p = create(&panel, fb);
    frame(p);
    if (!fb) {
        esp_display_present_engine_notify_te_isr(p, present_now_us());
    }
    wait_submit(&panel, 1); /* Owner does not pump/query/drain to cause submission. */
    assert(esp_display_present_engine_drain(p, 0) == ESP_ERR_TIMEOUT);
    if (fb) {
        frame(p);    /* Third buffer can queue while first is pending. */
    }
    complete(&panel);
    if (fb) {
        wait_submit(&panel, 2);
        complete(&panel);
    }
    OK(esp_display_present_engine_drain(p, 1000));
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.completed_frames == (fb ? 2u : 1u));
    pthread_mutex_lock(&panel.lock);
    panel.immediate = true;
    pthread_mutex_unlock(&panel.lock);
    frame(p);
    if (!fb) {
        esp_display_present_engine_notify_te(p, present_now_us());
    }
    wait_submit(&panel, fb ? 3 : 2);
    OK(esp_display_present_engine_drain(p, 1000));
    destroy(p, &panel);
}

static void bounded_join(void)
{
    panel_t panel;
    init_panel(&panel);
    panel.blocked = true;
    esp_display_present_engine_t *p = create(&panel, false);
    frame(p);
    esp_display_present_engine_notify_te(p, present_now_us());
    wait_submit(&panel, 1); /* Sender is inside a deliberately blocked driver call. */
    uint64_t start = present_now_ms();
    assert(esp_display_present_engine_stop(p, 5) == ESP_ERR_TIMEOUT);
    assert(present_now_ms() - start < 500);
    assert(esp_display_present_engine_destroy(p) == ESP_ERR_INVALID_STATE);
    esp_display_present_frame_request_t request = {0};
    assert(esp_display_present_engine_begin_next_frame(p, &request) == ESP_ERR_INVALID_STATE);
    pthread_mutex_lock(&panel.lock);
    assert(panel.quiesces == 0); /* No quiesce racing submit. */
    panel.release = true;
    panel.immediate = true;
    pthread_cond_broadcast(&panel.condition);
    pthread_mutex_unlock(&panel.lock);
    destroy(p, &panel);
}

static void rejection(void)
{
    panel_t panel;
    init_panel(&panel);
    panel.reject = true;
    esp_display_present_engine_t *p = create(&panel, false);
    frame(p);
    esp_display_present_engine_notify_te(p, present_now_us());
    wait_submit(&panel, 1);
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    /* A queued copy-progress wake may already have run a no-op service after
     * the rejected submit. last_service_error is a snapshot, not a sticky
     * rejection history. The driver is still rejecting and the frame must
     * remain pending without poisoning the accepted-operation fault. */
    assert(status.last_service_error == ESP_FAIL || status.last_service_error == ESP_OK);
    assert(status.first_error == ESP_OK && status.pending_frames == 1);
    pthread_mutex_lock(&panel.lock);
    panel.reject = false;
    pthread_mutex_unlock(&panel.lock);
    esp_display_present_engine_notify_te_isr(p, present_now_us());
    wait_submit(&panel, 2);
    complete(&panel);
    OK(esp_display_present_engine_drain(p, 1000));
    destroy(p, &panel);
}

static esp_display_present_engine_t *create_gram(panel_t *panel, uint32_t transfer_timeout)
{
    esp_display_present_engine_config_t config = {
        .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_REGION,
        .backend = {
            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2,
            .max_frames = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
            .transfer_capacity = 1, .deferred_submit = true, .panel = {panel, attach, submit, quiesce}
        },
        .timeout_ms = 20, .transfer_timeout_ms = transfer_timeout, .background_service = true
    };
    esp_display_present_engine_t *p = NULL;
    OK(esp_display_present_engine_create(&config, &p));
    return p;
}

static esp_display_present_lease_t gram_tile(esp_display_present_engine_t *p, unsigned y)
{
    esp_display_present_lease_t lease;
    OK(esp_display_present_engine_acquire_buffer(p, &lease));
    memset(lease.surface.pixels, 0xab, 16);
    esp_display_present_engine_area_t area = {0, y, 3, y + 1};
    OK(esp_display_present_engine_submit_buffer(p, &lease, &area, 8));
    return lease;
}

static void gram_driver_block_and_stop(void)
{
    panel_t panel;
    init_panel(&panel);
    panel.blocked = true;
    esp_display_present_engine_t *p = create_gram(&panel, 0);
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    esp_display_present_lease_t first = gram_tile(p, 0);
    wait_submit(&panel, 1);
    uint64_t start = present_now_ms();
    esp_display_present_lease_t second = gram_tile(p, 2);
    assert(present_now_ms() - start < 500);
    assert(first.surface.pixels != second.surface.pixels);
    OK(esp_display_present_engine_commit_frame(p, NULL));
    pthread_mutex_lock(&panel.lock);
    assert(panel.surface.pixels == first.surface.pixels); /* No conversion copy. */
    pthread_mutex_unlock(&panel.lock);
    assert(esp_display_present_engine_stop(p, 5) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_destroy(p) == ESP_ERR_INVALID_STATE);
    pthread_mutex_lock(&panel.lock);
    assert(panel.quiesces == 0 && panel.submissions == 1);
    panel.release = true;
    panel.immediate = true;
    pthread_cond_broadcast(&panel.condition);
    pthread_mutex_unlock(&panel.lock);
    OK(esp_display_present_engine_stop(p, 1000));
    assert(panel.submissions == 1); /* Closing does not submit queued tile #2. */
    destroy(p, &panel);
}

static void gram_zero_copy_backpressure(void)
{
    panel_t panel;
    init_panel(&panel);
    esp_display_present_engine_t *p = create_gram(&panel, 0);
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    esp_display_present_lease_t first = gram_tile(p, 0);
    wait_submit(&panel, 1);
    (void)gram_tile(p, 2);
    esp_display_present_lease_t unavailable;
    assert(esp_display_present_engine_acquire_buffer(p, &unavailable) == ESP_ERR_TIMEOUT);
    OK(esp_display_present_engine_commit_frame(p, NULL));
    pthread_mutex_lock(&panel.lock);
    assert(panel.surface.pixels == first.surface.pixels && panel.submissions == 1);
    pthread_mutex_unlock(&panel.lock);
    complete(&panel);
    wait_submit(&panel, 2);
    complete(&panel);
    OK(esp_display_present_engine_drain(p, 1000));
    pthread_mutex_lock(&panel.lock);
    panel.immediate = true;
    pthread_mutex_unlock(&panel.lock);
    frame(p); /* Both callbacks may occur before submit returns. */
    OK(esp_display_present_engine_drain(p, 1000));
    destroy(p, &panel);
}

static void gram_uncertain_keeps_pixels(void)
{
    panel_t panel;
    init_panel(&panel);
    panel.uncertain = true;
    esp_display_present_engine_t *p = create_gram(&panel, 0);
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    esp_display_present_lease_t source = gram_tile(p, 0);
    wait_submit(&panel, 1);
    esp_display_present_engine_status_t status;
    uint64_t start = present_now_ms();
    do {
        OK(esp_display_present_engine_get_status(p, &status));
        assert(present_now_ms() - start < 1000);
        present_pause();
    } while (status.first_error == ESP_OK);
    assert(status.first_error == ESP_FAIL);
    assert(esp_display_present_engine_stop(p, 5) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_destroy(p) == ESP_ERR_INVALID_STATE);
    pthread_mutex_lock(&panel.lock);
    assert(panel.surface.pixels == source.surface.pixels);
    assert(*(const uint16_t *)panel.surface.pixels == 0xabab);
    pthread_mutex_unlock(&panel.lock);
    complete(&panel); /* Real completion, rather than submit error, ends reads. */
    destroy(p, &panel);
}

static void gram_timeout_during_driver_call(void)
{
    panel_t panel;
    init_panel(&panel);
    panel.blocked = true;
    esp_display_present_engine_t *p = create_gram(&panel, 5);
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    esp_display_present_lease_t source = gram_tile(p, 0);
    wait_submit(&panel, 1);
    esp_display_present_engine_status_t status;
    uint64_t start = present_now_ms();
    do {
        OK(esp_display_present_engine_get_status(p, &status));
        assert(present_now_ms() - start < 1000);
        present_pause();
    } while (status.first_error == ESP_OK);
    assert(status.first_error == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_stop(p, 5) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&panel.lock);
    assert(panel.quiesces == 0 && panel.surface.pixels == source.surface.pixels);
    assert(*(const uint16_t *)panel.surface.pixels == 0xabab);
    panel.release = panel.immediate = true;
    pthread_cond_broadcast(&panel.condition);
    pthread_mutex_unlock(&panel.lock);
    destroy(p, &panel);
}

static void wait_te_stop(void)
{
    panel_t panel;
    init_panel(&panel);
    esp_display_present_engine_t *p = create(&panel, false);
    frame(p); /* No TE: worker must park, and stop must wake it. */
    destroy(p, &panel);
    assert(panel.submissions == 0);
}

static void failure_cleanup(void)
{
    panel_t panel;
    init_panel(&panel);
    esp_display_present_engine_config_t config = {.background_service = true, .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE,
                                                  .backend = {.panel = {&panel, attach, submit, quiesce}}
                                                 };
    esp_display_present_engine_t *p;
    assert(esp_display_present_engine_create(&config, &p) == ESP_ERR_INVALID_ARG && p == NULL);
    assert(panel.callback == NULL);
    pthread_cond_destroy(&panel.condition);
    pthread_mutex_destroy(&panel.lock);
}
int main(void)
{
    autonomous(false);
    autonomous(true);
    bounded_join();
    rejection();
    gram_driver_block_and_stop();
    gram_zero_copy_backpressure();
    gram_uncertain_keeps_pixels();
    gram_timeout_during_driver_call();
    wait_te_stop();
    failure_cleanup();
    puts("Scheduler: autonomous TE/FB, queued FB, early callback, bounded join and parked exit passed");
    return 0;
}
