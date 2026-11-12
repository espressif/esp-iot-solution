/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "core/present_engine.h"
#include "platform/present_os.h"
#include "test_panel.h"
#include "scan_panel.h"
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#define OK(x) assert((x) == ESP_OK)
static bool fake_clock = true;
static uint64_t clock_us;
uint64_t __real_present_now_us(void);
uint64_t __wrap_present_now_us(void)
{
    return fake_clock ? clock_us : __real_present_now_us();
}
static size_t allocation_count, fail_allocation;
void *__real_present_alloc(size_t);
void *__wrap_present_alloc(size_t size)
{
    if (fail_allocation && ++allocation_count == fail_allocation) {
        return NULL;
    }
    return __real_present_alloc(size);
}
static void allocation_rollback(void)
{
    for (size_t nth = 1; nth < 128; ++nth) {
        test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
        esp_display_present_engine_config_t config = {.backend = {.width = 4, .height = 4, .tile_rows = 2,
                                                                  .buffer_count = 2, .max_frames = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                                  .panel = test_panel_interface(&panel)
                                                                 }, .transfer_timeout_ms = 5
                                                     };
        allocation_count = 0;
        fail_allocation = nth;
        esp_display_present_engine_t *presenter = NULL;
        esp_err_t error = esp_display_present_engine_create(&config, &presenter);
        fail_allocation = 0;
        if (error == ESP_OK) {
            OK(esp_display_present_engine_stop(presenter, 1000));
            OK(esp_display_present_engine_destroy(presenter));
            return;
        }
        assert(error == ESP_ERR_NO_MEM && !presenter && !panel.callback);
    }
    assert(!"allocation sweep did not reach a successful create");
}
static esp_display_present_engine_t *create_adapter(esp_display_present_panel_adapter_t adapter, uint32_t timeout)
{
    esp_display_present_engine_config_t config = {.backend = {
            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 4,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = adapter
        },
        .transfer_timeout_ms = timeout
    };
    esp_display_present_engine_t *presenter;
    OK(esp_display_present_engine_create(&config, &presenter));
    const esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(esp_display_present_engine_begin_next_frame(presenter, &request));
    return presenter;
}
static esp_display_present_engine_t *create(test_panel_t *panel, uint32_t timeout)
{
    return create_adapter(test_panel_interface(panel), timeout);
}
static esp_err_t send(esp_display_present_engine_t *presenter, uint32_t y)
{
    esp_display_present_lease_t lease;
    OK(esp_display_present_engine_acquire_buffer(presenter, &lease));
    memset(lease.surface.pixels, 0xab, 16);
    esp_display_present_engine_area_t area = {0, y, 3, y + 1};
    return esp_display_present_engine_submit_buffer(presenter, &lease, &area, 8);
}
static esp_display_present_engine_receipt_t seal(esp_display_present_engine_t *presenter)
{
    esp_display_present_engine_receipt_t receipt;
    OK(esp_display_present_engine_commit_with_receipt(presenter, NULL, &receipt));
    return receipt;
}
static void destroy(esp_display_present_engine_t *presenter)
{
    OK(esp_display_present_engine_cancel_frame(presenter));
    OK(esp_display_present_engine_stop(presenter, 1000));
    OK(esp_display_present_engine_destroy(presenter));
}
static void unsealed_and_late(bool poll_before_done)
{
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    clock_us = 100000;
    esp_display_present_engine_t *presenter = create(&panel, 5);
    OK(send(presenter, 0));
    clock_us += 5000;
    if (poll_before_done) {
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(presenter, &status));
        assert(status.first_error == ESP_ERR_TIMEOUT && status.frame_active);
        esp_display_present_lease_t unused;
        assert(esp_display_present_engine_acquire_buffer(presenter, &unused) == ESP_ERR_TIMEOUT && !unused.id.value);
        assert(esp_display_present_engine_stop(presenter, 0) == ESP_ERR_TIMEOUT);
        assert(panel.count == 1 && panel.callback);
    }
    /* Source is read here, after the deadline and after failed stop. */
    test_panel_complete(&panel);
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(presenter, &status));
    assert(status.first_error == ESP_ERR_TIMEOUT && !status.committed_frames);
    destroy(presenter);
}
static void per_push_and_instances(void)
{
    test_panel_t a = {.width = 4, .height = 4, .bpp = 2};
    test_panel_t b = {.width = 4, .height = 4, .bpp = 2};
    clock_us = 200000;
    esp_display_present_engine_t *first = create(&a, 5);
    OK(send(first, 0));
    clock_us += 4000;
    test_panel_complete(&a); /* First push met its own deadline. */
    OK(send(first, 2));
    esp_display_present_engine_t *second = create(&b, 5);
    OK(send(second, 0));
    esp_display_present_engine_receipt_t one = seal(first), two = seal(second);
    clock_us += 4999;
    esp_display_present_frame_status_t status;
    OK(esp_display_present_engine_query_frame(first, one, &status));
    assert(!status.terminal && !status.src_released && status.result == ESP_OK);
    test_panel_complete(&b); /* The other instance must not inherit A's fault. */
    clock_us += 1;
    OK(esp_display_present_engine_query_frame(first, one, &status));
    assert(!status.terminal && !status.src_released && status.result == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_release_receipt(first, one) == ESP_ERR_INVALID_STATE);
    OK(esp_display_present_engine_query_frame(second, two, &status));
    assert(status.terminal && status.result == ESP_OK);
    test_panel_complete(&a);
    OK(esp_display_present_engine_query_frame(first, one, &status));
    assert(status.terminal && status.src_released && status.result == ESP_ERR_TIMEOUT && !status.present_confirmed);
    OK(esp_display_present_engine_release_receipt(first, one));
    OK(esp_display_present_engine_release_receipt(second, two));
    destroy(first);
    destroy(second);
}
static void rejection_and_early_done(void)
{
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2, .reject = true};
    clock_us = 300000;
    esp_display_present_engine_t *presenter = create(&panel, 5);
    esp_display_present_lease_t lease;
    OK(esp_display_present_engine_acquire_buffer(presenter, &lease));
    memset(lease.surface.pixels, 0xab, 16);
    esp_display_present_engine_area_t area = {0, 0, 3, 1};
    assert(esp_display_present_engine_submit_buffer(presenter, &lease, &area, 8) == ESP_FAIL);
    clock_us += 100000;
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(presenter, &status));
    assert(status.first_error == ESP_OK && !panel.count);
    panel.reject = false;
    panel.immediate = true;
    OK(esp_display_present_engine_submit_buffer(presenter, &lease, &area, 8));
    esp_display_present_engine_receipt_t receipt = seal(presenter);
    clock_us += 100000;
    esp_display_present_frame_status_t observed;
    OK(esp_display_present_engine_query_frame(presenter, receipt, &observed));
    assert(observed.terminal && observed.result == ESP_OK);
    OK(esp_display_present_engine_release_receipt(presenter, receipt));
    destroy(presenter);
    /* Zero retains the old opt-out behavior. */
    panel = (test_panel_t) {
        .width = 4, .height = 4, .bpp = 2
    };
    presenter = create(&panel, 0);
    OK(send(presenter, 0));
    clock_us += 1000000;
    OK(esp_display_present_engine_get_status(presenter, &status));
    assert(status.first_error == ESP_OK);
    test_panel_complete(&panel);
    destroy(presenter);
}

static esp_display_present_tx_result_t uncertain_submit(void *context, const esp_display_present_surface_t *surface,
                                                        const esp_display_present_rect_t *area, uint64_t cookie)
{
    esp_display_present_tx_result_t result = test_panel_submit(context, surface, area, cookie);
    assert(result.acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
    return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ESP_FAIL
    };
}
static void uncertain_retains_sources(void)
{
    clock_us = 600000;
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_panel_adapter_t adapter = test_panel_interface(&panel);
    adapter.submit = uncertain_submit;
    esp_display_present_engine_t *presenter = create_adapter(adapter, 5);
    OK(send(presenter, 0)); /* Accepted/uncertain consumes the lease. */
    esp_display_present_engine_receipt_t receipt = seal(presenter);
    clock_us += 5000;
    esp_display_present_frame_status_t status;
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(status.result == ESP_FAIL && !status.terminal && !status.src_released);
    assert(esp_display_present_engine_stop(presenter, 0) == ESP_ERR_TIMEOUT);
    assert(panel.callback && panel.count == 1);
    test_panel_complete(&panel);
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(status.result == ESP_FAIL && status.terminal && status.src_released);
    OK(esp_display_present_engine_release_receipt(presenter, receipt));
    destroy(presenter); /* Preserve the first driver failure, not later timeout. */
}
static void late_scanout_confirmation(bool poll_first)
{
    clock_us = 800000;
    scan_panel_t panel = {.stop_timeout = true};
    esp_display_present_engine_config_t config = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL,
                                                  .backend = {.width = 4, .height = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                              .max_frames = 4, .panel = scan_interface(&panel)
                                                             }, .transfer_timeout_ms = 5
                                                 };
    esp_display_present_engine_t *presenter;
    OK(esp_display_present_engine_create(&config, &presenter));
    esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(esp_display_present_engine_begin_next_frame(presenter, &request));
    esp_display_present_lease_t lease;
    OK(esp_display_present_engine_acquire_buffer(presenter, &lease));
    memset(lease.surface.pixels, 0xab, 32);
    esp_display_present_engine_area_t area = {0, 0, 3, 3};
    OK(esp_display_present_engine_submit_buffer(presenter, &lease, &area, 8));
    esp_display_present_engine_receipt_t receipt = seal(presenter);
    OK(esp_display_present_engine_service(presenter));
    assert(panel.cookie);
    clock_us += 5000;
    esp_display_present_frame_status_t status;
    if (poll_first) {
        OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
        assert(status.result == ESP_ERR_TIMEOUT && !status.terminal && !status.src_released);
        assert(esp_display_present_engine_stop(presenter, 0) == ESP_ERR_TIMEOUT);
    }
    scan_confirm(&panel); /* True ownership evidence must still be consumed. */
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(status.terminal && status.src_released && status.result == ESP_ERR_TIMEOUT && !status.present_confirmed);
    OK(esp_display_present_engine_release_receipt(presenter, receipt));
    panel.stop_timeout = false;
    destroy(presenter);
}

static void te_wait_is_not_transfer_time(void)
{
    clock_us = 900000;
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_engine_config_t config = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE,
                                                  .backend = {.width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2,
                                                              .max_frames = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = test_panel_interface(&panel),
                                                              .te_window_us = 100000, .te_budget_us = 1000
                                                             }, .transfer_timeout_ms = 5
                                                 };
    esp_display_present_engine_t *presenter;
    OK(esp_display_present_engine_create(&config, &presenter));
    esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(esp_display_present_engine_begin_next_frame(presenter, &request));
    OK(send(presenter, 0));
    OK(send(presenter, 2));
    esp_display_present_engine_receipt_t receipt = seal(presenter);
    clock_us += 1000000; /* No edge: still no physical submit. */
    esp_display_present_frame_status_t observed;
    OK(esp_display_present_engine_query_frame(presenter, receipt, &observed));
    assert(!panel.count && observed.result == ESP_OK && !observed.terminal);
    esp_display_present_engine_notify_te(presenter, clock_us);
    OK(esp_display_present_engine_service(presenter));
    assert(panel.count == 1);
    clock_us += 5000;
    OK(esp_display_present_engine_query_frame(presenter, receipt, &observed));
    assert(observed.result == ESP_ERR_TIMEOUT && !observed.terminal && !observed.src_released);
    test_panel_complete(&panel);
    OK(esp_display_present_engine_query_frame(presenter, receipt, &observed));
    assert(observed.terminal && observed.src_released && observed.result == ESP_ERR_TIMEOUT);
    OK(esp_display_present_engine_release_receipt(presenter, receipt));
    destroy(presenter);
}

/* Identity DMA is submitted on the caller. The background worker still has to
 * deliver the timeout fault; the owner must not observe it by polling. */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t condition;
    esp_display_present_panel_event_fn callback;
    void *user;
    uint64_t cookie;
    esp_display_present_surface_t source;
    present_task_t owner;
    bool submitted, expired;
} automatic_panel_t;
static esp_err_t automatic_attach(void *user, esp_display_present_panel_event_fn callback, void *context)
{
    automatic_panel_t *panel = user;
    panel->callback = callback;
    panel->user = context;
    panel->owner = present_current_task();
    return ESP_OK;
}
static esp_display_present_tx_result_t automatic_submit(void *user, const esp_display_present_surface_t *source, const esp_display_present_rect_t *area, uint64_t cookie)
{
    (void)area;
    automatic_panel_t *panel = user;
    pthread_mutex_lock(&panel->lock);
    assert(!panel->cookie);
    panel->cookie = cookie;
    panel->source = *source;
    panel->submitted = true;
    pthread_cond_broadcast(&panel->condition);
    pthread_mutex_unlock(&panel->lock);
    return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
}
static bool automatic_progress(void *user, bool isr)
{
    (void)isr;
    automatic_panel_t *panel = user;
    pthread_mutex_lock(&panel->lock);
    if (panel->submitted && panel->cookie && !present_same_task(panel->owner, present_current_task())) {
        panel->expired = true;
        pthread_cond_broadcast(&panel->condition);
    }
    pthread_mutex_unlock(&panel->lock);
    return false;
}
static esp_err_t automatic_stop(void *user, uint32_t timeout)
{
    (void)timeout;
    automatic_panel_t *panel = user;
    pthread_mutex_lock(&panel->lock);
    esp_err_t error = panel->cookie ? ESP_ERR_TIMEOUT : ESP_OK;
    if (error == ESP_OK) {
        panel->callback = NULL;
    }
    pthread_mutex_unlock(&panel->lock);
    return error;
}
static void automatic_backend_worker(void)
{
    fake_clock = false;
    automatic_panel_t panel = {.lock = PTHREAD_MUTEX_INITIALIZER, .condition = PTHREAD_COND_INITIALIZER};
    esp_display_present_engine_config_t config = {.backend = {.width = 4, .height = 4, .tile_rows = 2,
                                                              .buffer_count = 2, .max_frames = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .async_copy = true,
                                                              .panel = {&panel, automatic_attach, automatic_submit, automatic_stop},
                                                              .on_progress = automatic_progress, .progress_user = &panel
                                                             },
                                                  .timeout_ms = 1000, .transfer_timeout_ms = 10, .background_service = true
                                                 };
    esp_display_present_engine_t *presenter;
    OK(esp_display_present_engine_create(&config, &presenter));
    esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(esp_display_present_engine_begin_next_frame(presenter, &request));
    OK(send(presenter, 0));
    esp_display_present_engine_receipt_t receipt = seal(presenter);
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 2;
    pthread_mutex_lock(&panel.lock);
    while (!panel.expired) {
        assert(!pthread_cond_timedwait(&panel.condition, &panel.lock, &deadline));
    }
    esp_display_present_surface_t retained = panel.source;
    pthread_mutex_unlock(&panel.lock);
    /* A real fault notification arrived with no owner pump/status call. */
    esp_display_present_frame_status_t status;
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(status.result == ESP_ERR_TIMEOUT && !status.src_released && !status.terminal);
    assert(esp_display_present_engine_stop(presenter, 1000) == ESP_ERR_TIMEOUT);
    uint16_t pixel;
    memcpy(&pixel, retained.pixels, sizeof(pixel));
    assert(pixel == 0xabab);
    pthread_mutex_lock(&panel.lock);
    esp_display_present_backend_event_t done = {.kind = ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, .transaction_cookie = panel.cookie};
    panel.cookie = 0;
    esp_display_present_panel_event_fn callback = panel.callback;
    void *user = panel.user;
    pthread_mutex_unlock(&panel.lock);
    (void)callback(user, &done, false);
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(status.terminal && status.src_released && status.result == ESP_ERR_TIMEOUT);
    OK(esp_display_present_engine_release_receipt(presenter, receipt));
    destroy(presenter);
    pthread_mutex_destroy(&panel.lock);
    pthread_cond_destroy(&panel.condition);
}
/* Exact fake-clock boundaries, with completion before/after task observation.
 * A failed STOP must preserve the callback and all still-accessible sources. */
static void exhausted_deadline_boundaries(void)
{
    for (unsigned tiles = 1; tiles <= 2; ++tiles) {
        for (unsigned age = 4999; age <= 5001; ++age) {
            for (unsigned poll_first = 0; poll_first < 2; ++poll_first) {
                for (unsigned cancel_first = 0; cancel_first < 2; ++cancel_first) {
                    clock_us = 100000;
                    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
                    esp_display_present_engine_config_t cfg = {
                        .backend = {
                            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = tiles,
                            .max_frames = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                            .panel = test_panel_interface(&panel)
                        },
                        .transfer_timeout_ms = 5,
                    };
                    esp_display_present_engine_t *presenter;
                    OK(esp_display_present_engine_create(&cfg, &presenter));
                    esp_display_present_frame_request_t request = {.full_redraw = true};
                    OK(esp_display_present_engine_begin_next_frame(presenter, &request));
                    for (unsigned i = 0; i < tiles; ++i) {
                        OK(send(presenter, i * 2));
                    }
                    esp_display_present_lease_t unused;
                    assert(esp_display_present_engine_acquire_buffer(presenter, &unused) == ESP_ERR_TIMEOUT && !unused.id.value);
                    clock_us += age;
                    esp_display_present_engine_status_t status;
                    if (poll_first) {
                        OK(esp_display_present_engine_get_status(presenter, &status));
                    }
                    if (cancel_first) {
                        assert(esp_display_present_engine_cancel_frame(presenter) == ESP_ERR_TIMEOUT);
                        assert(esp_display_present_engine_cancel_frame(presenter) == ESP_ERR_TIMEOUT);
                        assert(esp_display_present_engine_stop(presenter, 0) == ESP_ERR_TIMEOUT);
                        assert(esp_display_present_engine_destroy(presenter) == ESP_ERR_INVALID_STATE);
                    }
                    assert(panel.count == tiles && panel.callback && panel.submitted == tiles);
                    for (unsigned i = 0; i < tiles; ++i) {
                        const uint8_t *source = panel.pending[i].surface.pixels;
                        for (unsigned byte = 0; byte < 16; ++byte) {
                            assert(source[byte] == 0xab);
                        }
                    }
                    for (unsigned i = 0; i < tiles; ++i) {
                        test_panel_complete(&panel);
                    }
                    OK(esp_display_present_engine_get_status(presenter, &status));
                    assert(status.first_error == (age >= 5000 ? ESP_ERR_TIMEOUT : ESP_OK));
                    OK(esp_display_present_engine_cancel_frame(presenter));
                    OK(esp_display_present_engine_stop(presenter, 1000));
                    assert(panel.submitted == tiles && !panel.callback && !panel.count);
                    OK(esp_display_present_engine_destroy(presenter));
                }
            }
        }
    }
}

static void partially_completed_cancel(void)
{
    for (unsigned late = 0; late < 2; ++late) {
        clock_us = 200000;
        test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
        esp_display_present_engine_t *presenter = create(&panel, 5);
        OK(send(presenter, 0));
        OK(send(presenter, 2));
        clock_us += 4000;
        test_panel_complete(&panel);
        assert(panel.count == 1 && panel.submitted == 2);
        const uint8_t *source = panel.pending[0].surface.pixels;
        assert(esp_display_present_engine_cancel_frame(presenter) == ESP_ERR_TIMEOUT);
        assert(esp_display_present_engine_cancel_frame(presenter) == ESP_ERR_TIMEOUT);
        assert(esp_display_present_engine_stop(presenter, 0) == ESP_ERR_TIMEOUT);
        assert(esp_display_present_engine_destroy(presenter) == ESP_ERR_INVALID_STATE);
        assert(panel.callback && panel.count == 1 && panel.submitted == 2);
        for (unsigned byte = 0; byte < 16; ++byte) {
            assert(source[byte] == 0xab);
        }
        clock_us += late ? 1001 : 999;
        test_panel_complete(&panel);
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(presenter, &status));
        assert(status.first_error == (late ? ESP_ERR_TIMEOUT : ESP_OK));
        OK(esp_display_present_engine_cancel_frame(presenter));
        OK(esp_display_present_engine_stop(presenter, 1000));
        assert(!panel.callback && !panel.count && panel.submitted == 2);
        OK(esp_display_present_engine_destroy(presenter));
    }
}

int main(void)
{
    exhausted_deadline_boundaries();
    partially_completed_cancel();
    allocation_rollback();
    unsealed_and_late(false);
    unsealed_and_late(true);
    per_push_and_instances();
    rejection_and_early_done();
    uncertain_retains_sources();
    late_scanout_confirmation(false);
    late_scanout_confirmation(true);
    te_wait_is_not_transfer_time();
    automatic_backend_worker();
    puts("transfer deadlines: physical-submit origin, retained DMA, per-push isolation, retry, early/late completion and backend worker wake passed");
}
