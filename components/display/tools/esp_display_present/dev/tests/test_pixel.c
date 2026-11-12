/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_engine.h"
#include "platform/present_os.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static _Atomic unsigned priority_set_calls;
static _Atomic unsigned priority_set_value;
static _Thread_local unsigned simulated_priority;
static pthread_mutex_t priority_gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t priority_gate_changed = PTHREAD_COND_INITIALIZER;
static bool priority_gate_blocked, priority_gate_entered;
void __real_present_task_set_priority(unsigned priority);
unsigned __wrap_present_task_priority(void)
{
    return simulated_priority;
}
void __wrap_present_task_set_priority(unsigned priority)
{
    atomic_store(&priority_set_value, priority);
    atomic_fetch_add(&priority_set_calls, 1);
    pthread_mutex_lock(&priority_gate_lock);
    if (priority_gate_blocked) {
        priority_gate_entered = true;
        pthread_cond_signal(&priority_gate_changed);
        while (priority_gate_blocked) {
            pthread_cond_wait(&priority_gate_changed, &priority_gate_lock);
        }
    }
    pthread_mutex_unlock(&priority_gate_lock);
    __real_present_task_set_priority(priority);
}

typedef struct {
    esp_display_present_pixel_engine_t *engine;
    esp_display_present_copy_request_t request;
    esp_display_present_copy_ticket_t ticket;
} priority_submit_t;

static void *priority_submit(void *user)
{
    priority_submit_t *submit = user;
    simulated_priority = 6;
    assert(esp_display_present_pixel_submit(submit->engine, &submit->request, &submit->ticket) == ESP_OK);
    return NULL;
}

/* The first accepted asynchronous submitter, not create() or an invalid
 * request, determines the worker's scheduling priority. Later producers must
 * not silently change it during a handoff. */
static void async_worker_priority(void)
{
    uint8_t src[2] = {1, 2}, dst[2] = {0};
    esp_display_present_pixel_config_t cfg = {.max_jobs = 1, .asynchronous = true};
    priority_submit_t submit = {.request = {
            .src = {src, sizeof(src), {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
            .dst = {dst, sizeof(dst), {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
            .src_area = {0, 0, 1, 1},
        }
    };
    atomic_store(&priority_set_calls, 0);
    assert(esp_display_present_pixel_create(&cfg, &submit.engine) == ESP_OK);
    esp_display_present_copy_request_t invalid = submit.request;
    invalid.src.pixels = NULL;
    assert(esp_display_present_pixel_submit(submit.engine, &invalid, &submit.ticket) == ESP_ERR_INVALID_ARG);
    assert(atomic_load(&priority_set_calls) == 0);

    pthread_t producer;
    assert(pthread_create(&producer, NULL, priority_submit, &submit) == 0);
    assert(pthread_join(producer, NULL) == 0);
    esp_display_present_copy_status_t status;
    assert(esp_display_present_pixel_wait(submit.engine, submit.ticket, UINT32_MAX, &status) == ESP_OK);
    assert(atomic_load(&priority_set_calls) == 1);
    assert(atomic_load(&priority_set_value) == 6);
    assert(status.result == ESP_OK && memcmp(src, dst, sizeof(src)) == 0);
    assert(esp_display_present_pixel_release(submit.engine, submit.ticket) == ESP_OK);
    assert(esp_display_present_pixel_submit(submit.engine, &submit.request, &submit.ticket) == ESP_OK);
    assert(esp_display_present_pixel_wait(submit.engine, submit.ticket, UINT32_MAX, &status) == ESP_OK);
    assert(esp_display_present_pixel_release(submit.engine, submit.ticket) == ESP_OK);
    assert(atomic_load(&priority_set_calls) == 1);
    assert(esp_display_present_pixel_stop(submit.engine, UINT32_MAX) == ESP_OK);
    assert(esp_display_present_pixel_destroy(submit.engine) == ESP_OK);

    cfg.asynchronous = false;
    assert(esp_display_present_pixel_create(&cfg, &submit.engine) == ESP_OK);
    assert(esp_display_present_pixel_submit(submit.engine, &submit.request, &submit.ticket) == ESP_OK);
    assert(esp_display_present_pixel_release(submit.engine, submit.ticket) == ESP_OK);
    assert(esp_display_present_pixel_stop(submit.engine, UINT32_MAX) == ESP_OK);
    assert(esp_display_present_pixel_destroy(submit.engine) == ESP_OK);
    assert(atomic_load(&priority_set_calls) == 1);
}

/* stop must keep the worker and buffers alive while its first scheduling
 * adjustment is in progress, and must be retryable after the worker finishes. */
static void stop_during_priority_setup(void)
{
    uint8_t src[2] = {0x12, 0x34}, dst[2] = {0};
    esp_display_present_pixel_config_t cfg = {.max_jobs = 1, .asynchronous = true};
    esp_display_present_copy_request_t request = {
        .src = {src, sizeof(src), {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
        .dst = {dst, sizeof(dst), {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
        .src_area = {0, 0, 1, 1},
    };
    esp_display_present_pixel_engine_t *engine;
    assert(esp_display_present_pixel_create(&cfg, &engine) == ESP_OK);
    pthread_mutex_lock(&priority_gate_lock);
    priority_gate_entered = false;
    priority_gate_blocked = true;
    pthread_mutex_unlock(&priority_gate_lock);
    simulated_priority = 4;
    esp_display_present_copy_ticket_t ticket;
    assert(esp_display_present_pixel_submit(engine, &request, &ticket) == ESP_OK);
    pthread_mutex_lock(&priority_gate_lock);
    while (!priority_gate_entered) {
        pthread_cond_wait(&priority_gate_changed, &priority_gate_lock);
    }
    pthread_mutex_unlock(&priority_gate_lock);
    assert(esp_display_present_pixel_stop(engine, 0) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_pixel_destroy(engine) == ESP_ERR_INVALID_STATE);
    pthread_mutex_lock(&priority_gate_lock);
    priority_gate_blocked = false;
    pthread_cond_signal(&priority_gate_changed);
    pthread_mutex_unlock(&priority_gate_lock);
    assert(esp_display_present_pixel_stop(engine, UINT32_MAX) == ESP_OK);
    esp_display_present_copy_status_t status;
    assert(esp_display_present_pixel_wait(engine, ticket, 0, &status) == ESP_OK);
    assert(status.result == ESP_OK && memcmp(src, dst, sizeof(src)) == 0);
    assert(esp_display_present_pixel_release(engine, ticket) == ESP_OK);
    assert(esp_display_present_pixel_destroy(engine) == ESP_OK);
}

static esp_display_present_pixel_engine_t *make_engine(size_t jobs)
{
    esp_display_present_pixel_engine_t *e;
    esp_display_present_pixel_config_t config = { .max_jobs = jobs };
    assert(esp_display_present_pixel_create(&config, &e) == ESP_OK);
    return e;
}

static void destroy(esp_display_present_pixel_engine_t *e)
{
    assert(esp_display_present_pixel_stop(e, 0) == ESP_OK);
    assert(esp_display_present_pixel_destroy(e) == ESP_OK);
}

static void pixels_and_padding(bool asynchronous)
{
    /* Independently enumerated expected clockwise rotations of 3 x 2. */
    const uint8_t expected[4][6] = {
        {11, 12, 13, 21, 22, 23}, {21, 11, 22, 12, 23, 13},
        {23, 22, 21, 13, 12, 11}, {13, 23, 12, 22, 11, 21},
    };
    esp_display_present_pixel_engine_t *e;
    esp_display_present_pixel_config_t config = {.max_jobs = 1, .asynchronous = asynchronous};
    assert(esp_display_present_pixel_create(&config, &e) == ESP_OK);
    for (int format = ESP_DISPLAY_PRESENT_FORMAT_RGB565; format <= ESP_DISPLAY_PRESENT_FORMAT_RGB888; ++format) {
        size_t bpp = format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
        for (int swap = 0; swap <= (format == ESP_DISPLAY_PRESENT_FORMAT_RGB565); ++swap) {
            for (int rot = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0; rot <= ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270; ++rot) {
                uint8_t src[128], snapshot[128], dst[160], reference[160];
                memset(src, 0xA5, sizeof(src));
                memset(dst, 0xCD, sizeof(dst));
                memset(reference, 0xCD, sizeof(reference));
                for (size_t y = 0; y < 2; ++y) {
                    for (size_t x = 0; x < 3; ++x) {
                        size_t at = (y + 1) * 6 * bpp + (x + 1) * bpp;
                        src[at] = (uint8_t)(11 + y * 10 + x);
                        src[at + 1] = 0x71;
                        if (bpp == 3) {
                            src[at + 2] = 0x93;
                        }
                    }
                }
                memcpy(snapshot, src, sizeof(src));
                uint32_t w = (rot % 2) ? 2 : 3, h = (rot % 2) ? 3 : 2;
                for (uint32_t y = 0; y < h; ++y) {
                    for (uint32_t x = 0; x < w; ++x) {
                        size_t at = (y + 2) * 8 * bpp + (x + 1) * bpp;
                        reference[at] = swap ? 0x71 : expected[rot][y * w + x];
                        reference[at + 1] = swap ? expected[rot][y * w + x] : 0x71;
                        if (bpp == 3) {
                            reference[at + 2] = 0x93;
                        }
                    }
                }
                esp_display_present_copy_request_t r = {
                    .src = { src, sizeof(src), { 5, 4, 6 * bpp, format, ESP_DISPLAY_PRESENT_ORDER_NATIVE } },
                    .dst = { dst, sizeof(dst), { 6, 6, 8 * bpp, format, swap } },
                    .src_area = { 1, 1, 3, 2 }, .dst_x = 1, .dst_y = 2,
                    .rotation = rot,
                };
                esp_display_present_copy_ticket_t ticket;
                assert(esp_display_present_pixel_submit(e, &r, &ticket) == ESP_OK);
                esp_display_present_copy_status_t status;
                assert(esp_display_present_pixel_wait(e, ticket, UINT32_MAX, &status) == ESP_OK);
                assert(status.state == ESP_DISPLAY_PRESENT_COPY_SUCCEEDED && status.result == ESP_OK);
                assert(memcmp(src, snapshot, sizeof(src)) == 0);
                assert(memcmp(dst, reference, sizeof(dst)) == 0);
                assert(esp_display_present_pixel_release(e, ticket) == ESP_OK);
            }
        }
    }
    assert(esp_display_present_pixel_stop(e, UINT32_MAX) == ESP_OK);
    assert(esp_display_present_pixel_destroy(e) == ESP_OK);
}

static void validation_and_records(void)
{
    uint8_t src[32] = {0}, dst[32];
    memset(dst, 0xB6, sizeof(dst));
    esp_display_present_copy_request_t valid = {
        .src = { src, sizeof(src), { 2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE } },
        .dst = { dst, sizeof(dst), { 2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE } },
        .src_area = { 0, 0, 2, 2 },
    };
    esp_display_present_pixel_engine_t *a = make_engine(1), *b = make_engine(1);
    for (int variant = 0; variant < 10; ++variant) {
        esp_display_present_copy_request_t r = valid;
        switch (variant) {
        case 0:
            r.src.layout.stride_bytes = 5;
            break;
        case 1:
            r.dst.layout.stride_bytes = 5;
            break;
        case 2:
            r.src.capacity_bytes = 7;
            break;
        case 3:
            r.dst.capacity_bytes = 7;
            break;
        case 4:
            r.dst.pixels = src + 2;
            break;
        case 5:
            r.src_area.x = UINT32_MAX;
            break;
        case 6:
            r.dst_x = UINT32_MAX;
            break;
        case 7:
            r.src.layout.stride_bytes = SIZE_MAX - 1;
            r.src.layout.height = 3;
            break;
        case 8:
            r.rotation = (esp_display_present_engine_rotation_t)99;
            break;
        case 9:
            r.src.layout.format = (esp_display_present_engine_pixel_format_t)99;
            break;
        }
        esp_display_present_copy_ticket_t t = {123};
        assert(esp_display_present_pixel_submit(a, &r, &t) == ESP_ERR_INVALID_ARG);
        assert(t.value == 0);
        for (size_t i = 0; i < sizeof(dst); ++i) {
            assert(dst[i] == 0xB6);
        }
    }
    esp_display_present_copy_ticket_t first, second;
    assert(esp_display_present_pixel_submit(a, &valid, &first) == ESP_OK);
    assert(esp_display_present_pixel_submit(a, &valid, &second) == ESP_ERR_NO_MEM);
    assert(second.value == 0);
    esp_display_present_copy_status_t status;
    assert(esp_display_present_pixel_query(b, first, &status) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_pixel_release(a, first) == ESP_OK);
    assert(esp_display_present_pixel_submit(a, &valid, &second) == ESP_OK);
    assert(first.value != second.value);
    assert(esp_display_present_pixel_query(a, first, &status) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_pixel_stop(a, 0) == ESP_OK);
    assert(esp_display_present_pixel_destroy(a) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_pixel_submit(a, &valid, &first) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_pixel_release(a, second) == ESP_OK);
    destroy(a);
    destroy(b);
}

static bool on_pixel_done(void *user, esp_display_present_copy_ticket_t ticket,
                          const esp_display_present_copy_status_t *status, bool from_isr)
{
    size_t *calls = user;
    assert(!from_isr);
    assert(ticket.value != 0);
    assert(status && status->state == ESP_DISPLAY_PRESENT_COPY_SUCCEEDED && status->result == ESP_OK);
    ++(*calls);
    return false;
}

static void in_place_and_events(void)
{
    size_t events = 0;
    esp_display_present_pixel_engine_t *e;
    esp_display_present_pixel_config_t config = { .max_jobs = 2, .on_event = on_pixel_done, .user = &events };
    assert(esp_display_present_pixel_create(&config, &e) == ESP_OK);

    uint8_t buf[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    esp_display_present_copy_request_t r = {
        .src = { buf, sizeof(buf), { 2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE } },
        .dst = { buf, sizeof(buf), { 2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_SWAPPED } },
        .src_area = { 0, 0, 2, 2 },
        .dst_x = 0, .dst_y = 0, .rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0,
    };
    esp_display_present_copy_ticket_t t;
    assert(esp_display_present_pixel_submit(e, &r, &t) == ESP_OK);
    assert(events == 1);
    const uint8_t expected[8] = { 0x22, 0x11, 0x44, 0x33, 0x66, 0x55, 0x88, 0x77 };
    assert(memcmp(buf, expected, sizeof(buf)) == 0);
    assert(esp_display_present_pixel_release(e, t) == ESP_OK);

    /* In-place with rotation must be rejected. */
    r.rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_180;
    assert(esp_display_present_pixel_submit(e, &r, &t) == ESP_ERR_INVALID_ARG);
    destroy(e);
}

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool entered, leave;
    esp_display_present_pixel_engine_t *engine;
    esp_display_present_copy_request_t request;
} callback_gate_t;

static bool blocking_callback(void *user, esp_display_present_copy_ticket_t ticket,
                              const esp_display_present_copy_status_t *status, bool isr)
{
    callback_gate_t *gate = user;
    assert(!isr && status->state == ESP_DISPLAY_PRESENT_COPY_SUCCEEDED);
    esp_display_present_copy_status_t queried;
    assert(esp_display_present_pixel_query(gate->engine, ticket, &queried) == ESP_OK);
    assert(esp_display_present_pixel_release(gate->engine, ticket) == ESP_OK);
    /* Neither self-stop nor recursive submit may deadlock this callback. */
    assert(esp_display_present_pixel_stop(gate->engine, UINT32_MAX) == ESP_ERR_INVALID_STATE);
    esp_display_present_copy_ticket_t nested;
    assert(esp_display_present_pixel_submit(gate->engine, &gate->request, &nested) == ESP_ERR_NO_MEM);
    pthread_mutex_lock(&gate->lock);
    gate->entered = true;
    pthread_cond_signal(&gate->changed);
    while (!gate->leave) {
        pthread_cond_wait(&gate->changed, &gate->lock);
    }
    pthread_mutex_unlock(&gate->lock);
    return false;
}

static void *callback_submitter(void *arg)
{
    callback_gate_t *gate = arg;
    esp_display_present_copy_ticket_t ticket;
    assert(esp_display_present_pixel_submit(gate->engine, &gate->request, &ticket) == ESP_OK);
    return NULL;
}

static void callback_lifetime(bool asynchronous)
{
    uint8_t src[2] = {0x12, 0x34}, dst[2] = {0};
    callback_gate_t gate = { .lock = PTHREAD_MUTEX_INITIALIZER,
                             .changed = PTHREAD_COND_INITIALIZER,
                             .request = { .src = { src, 2, {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE} },
                                          .dst = { dst, 2, {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE} },
                                          .src_area = {0, 0, 1, 1}
                                        }
                           };
    esp_display_present_pixel_config_t config = { .max_jobs = 1, .on_event = blocking_callback, .user = &gate, .asynchronous = asynchronous };
    assert(esp_display_present_pixel_create(&config, &gate.engine) == ESP_OK);
    pthread_t submitter;
    assert(pthread_create(&submitter, NULL, callback_submitter, &gate) == 0);
    pthread_mutex_lock(&gate.lock);
    while (!gate.entered) {
        pthread_cond_wait(&gate.changed, &gate.lock);
    }
    pthread_mutex_unlock(&gate.lock);
    /* Ticket is already recycled, but user callback still owns its context. */
    assert(esp_display_present_pixel_stop(gate.engine, 0) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&gate.lock);
    gate.leave = true;
    pthread_cond_signal(&gate.changed);
    pthread_mutex_unlock(&gate.lock);
    assert(pthread_join(submitter, NULL) == 0);
    assert(memcmp(src, dst, 2) == 0);
    assert(esp_display_present_pixel_stop(gate.engine, UINT32_MAX) == ESP_OK);
    assert(esp_display_present_pixel_destroy(gate.engine) == ESP_OK);
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.lock);
}

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool entered, leave;
    unsigned calls;
} executor_gate_t;

static esp_err_t delayed_executor(void *user, const esp_display_present_copy_request_t *r)
{
    executor_gate_t *gate = user;
    pthread_mutex_lock(&gate->lock);
    gate->calls++;
    gate->entered = true;
    pthread_cond_broadcast(&gate->changed);
    while (!gate->leave) {
        pthread_cond_wait(&gate->changed, &gate->lock);
    }
    pthread_mutex_unlock(&gate->lock);
    /* Test executor intentionally supports only these two-byte requests. */
    memcpy(r->dst.pixels, r->src.pixels, 2);
    return ESP_OK;
}

static void queued_lifetime(void)
{
    executor_gate_t gate = {.lock = PTHREAD_MUTEX_INITIALIZER,
                            .changed = PTHREAD_COND_INITIALIZER
                           };
    uint8_t src[3][2] = {{1, 2}, {3, 4}, {5, 6}}, dst[2] = {0};
    esp_display_present_pixel_config_t cfg = {.max_jobs = 3, .asynchronous = true,
                                              .execute = delayed_executor, .execute_user = &gate
                                             };
    esp_display_present_pixel_engine_t *e;
    assert(esp_display_present_pixel_create(&cfg, &e) == ESP_OK);
    esp_display_present_copy_request_t r = {
        .src = {src[0], 2, {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
        .dst = {dst, 2, {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
        .src_area = {0, 0, 1, 1}
    };
    esp_display_present_copy_ticket_t tickets[3], extra;
    assert(esp_display_present_pixel_submit(e, &r, &tickets[0]) == ESP_OK);
    pthread_mutex_lock(&gate.lock);
    while (!gate.entered) {
        pthread_cond_wait(&gate.changed, &gate.lock);
    }
    pthread_mutex_unlock(&gate.lock);
    for (unsigned i = 1; i < 3; ++i) {
        r.src.pixels = src[i];
        assert(esp_display_present_pixel_submit(e, &r, &tickets[i]) == ESP_OK);
    }
    assert(esp_display_present_pixel_submit(e, &r, &extra) == ESP_ERR_NO_MEM && !extra.value);
    /* Mutating the descriptor after submit must not alter queued requests. */
    r.src.pixels = NULL;
    esp_display_present_copy_status_t status;
    assert(esp_display_present_pixel_wait(e, tickets[0], 2, &status) == ESP_ERR_TIMEOUT);
    assert(status.state == ESP_DISPLAY_PRESENT_COPY_RUNNING);
    assert(esp_display_present_pixel_cancel(e, tickets[2], 0, &status) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_pixel_release(e, tickets[2]) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_pixel_stop(e, 0) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_pixel_destroy(e) == ESP_ERR_INVALID_STATE);
    r.src.pixels = src[0];
    assert(esp_display_present_pixel_submit(e, &r, &extra) == ESP_ERR_INVALID_STATE);
    pthread_mutex_lock(&gate.lock);
    gate.leave = true;
    pthread_cond_broadcast(&gate.changed);
    pthread_mutex_unlock(&gate.lock);
    assert(esp_display_present_pixel_stop(e, UINT32_MAX) == ESP_OK);
    assert(gate.calls == 2);
    assert(memcmp(dst, src[1], 2) == 0); /* FIFO; canceled third never wrote. */
    assert(esp_display_present_pixel_destroy(e) == ESP_ERR_INVALID_STATE); /* receipts survive */
    for (unsigned i = 0; i < 3; ++i) {
        assert(esp_display_present_pixel_wait(e, tickets[i], 0, &status) == ESP_OK);
        assert(status.result == (i == 2 ? ESP_ERR_INVALID_STATE : ESP_OK));
        assert(esp_display_present_pixel_release(e, tickets[i]) == ESP_OK);
        assert(esp_display_present_pixel_query(e, tickets[i], &status) == ESP_ERR_INVALID_STATE);
    }
    assert(esp_display_present_pixel_destroy(e) == ESP_OK);
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.lock);
}

int main(void)
{
    async_worker_priority();
    stop_during_priority_setup();
    pixels_and_padding(false);
    pixels_and_padding(true);
    queued_lifetime();
    validation_and_records();
    in_place_and_events();
    callback_lifetime(false);
    callback_lifetime(true);
    puts("PASS pixel: RGB565/RGB888, all rotations, swap, in-place, padding, source integrity, invalid layouts, token ownership");
    return 0;
}
