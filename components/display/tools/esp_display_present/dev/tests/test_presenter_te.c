/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "sync_panel.h"
#include "../../src/platform/te/present_te_timer.h"
#include <stdio.h>
#define OK(x) assert((x) == ESP_OK)
static bool unavailable_timer;
esp_err_t __real_present_te_timer_create(present_te_timer_callback_t, void *, present_te_timer_t **);
esp_err_t __wrap_present_te_timer_create(present_te_timer_callback_t callback, void *user, present_te_timer_t **out)
{
    if (unavailable_timer) {
        *out = NULL;
        return ESP_ERR_NOT_SUPPORTED;
    }
    return __real_present_te_timer_create(callback, user, out);
}

static esp_display_present_engine_t *create(sync_panel_t *s, uint32_t timeout)
{
    esp_display_present_engine_config_t config = {.backend = {
            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = sync_panel_interface(s),
            .te_window_us = 1000000, .te_budget_us = 1000
        },
        .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE, .timeout_ms = timeout
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
        memset(lease.surface.pixels, 0xab, lease.surface.capacity_bytes);
        esp_display_present_engine_area_t area = {0, y, 3, y + 1};
        OK(esp_display_present_engine_submit_buffer(p, &lease, &area, 8));
    }
    OK(esp_display_present_engine_commit_frame(p, NULL));
}

static void destroy(esp_display_present_engine_t *p, sync_panel_t *s)
{
    OK(esp_display_present_engine_stop(p, 0));
    OK(esp_display_present_engine_destroy(p));
    sync_panel_deinit(s);
}

static void pressure(void)
{
    for (unsigned timeout = 0; timeout <= 5; timeout += 5) {
        sync_panel_t s;
        sync_panel_init(&s);
        esp_display_present_engine_t *p = create(&s, timeout);
        frame(p);
        esp_display_present_frame_request_t request = {0};
        uint64_t start = test_now_us();
        assert(esp_display_present_engine_begin_next_frame(p, &request) == ESP_ERR_TIMEOUT);
        assert(test_now_us() - start < 500000); /* One bounded admission budget. */
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(p, &status));
        assert(!status.frame_active && status.pending_frames == 1 && status.first_error == ESP_OK);
        pthread_t thread = sync_start_worker(&s, p);
        OK(esp_display_present_engine_drain(p, 1000));
        assert(pthread_join(thread, NULL) == 0);
        OK(esp_display_present_engine_begin_next_frame(p, &request));
        OK(esp_display_present_engine_cancel_frame(p));
        destroy(p, &s);
    }
    sync_panel_t s;
    sync_panel_init(&s);
    esp_display_present_engine_t *p = create(&s, 1000);
    frame(p);
    pthread_t thread = sync_start_worker(&s, p);
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    assert(pthread_join(thread, NULL) == 0);
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.completed_frames == 1 && status.frame_active);
    OK(esp_display_present_engine_cancel_frame(p));
    destroy(p, &s);
}

static void rejection_and_fault(void)
{
    sync_panel_t s;
    sync_panel_init(&s);
    esp_display_present_engine_t *p = create(&s, 0);
    frame(p);
    s.panel.reject = true;
    esp_display_present_engine_notify_te(p, test_now_us());
    assert(esp_display_present_engine_service(p) == ESP_FAIL);
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.first_error == ESP_OK && status.pending_frames == 1);
    s.panel.reject = false;
    OK(esp_display_present_engine_service(p));
    assert(s.panel.submitted == 0);
    pthread_t thread = sync_start_worker(&s, p);
    OK(esp_display_present_engine_drain(p, 1000));
    assert(pthread_join(thread, NULL) == 0);
    frame(p);
    s.uncertain = true;
    esp_display_present_engine_notify_te_isr(p, test_now_us());
    assert(esp_display_present_engine_service(p) == ESP_FAIL);
    assert(esp_display_present_engine_stop(p, 0) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_destroy(p) == ESP_ERR_INVALID_STATE);
    sync_complete(&s);
    OK(esp_display_present_engine_stop(p, 0));
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.first_error == ESP_FAIL && !status.pending_frames);
    OK(esp_display_present_engine_destroy(p));
    sync_panel_deinit(&s);
}

static void *wrong_owner(void *context)
{
    assert(esp_display_present_engine_service(context) == ESP_ERR_INVALID_STATE);
    return NULL;
}

static void io_modes(void)
{
    const esp_display_present_present_mode_t modes[] = {ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE, ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC, ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO, ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO};
    for (size_t i = 0; i < 4; i++) {
        sync_panel_t panel;
        sync_panel_init(&panel);
        esp_display_present_engine_config_t cfg = {.backend = {
                .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
                .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = sync_panel_interface(&panel),
                .te_window_us = i == 3 ? 0 : 1000000, .te_budget_us = i == 3 ? 0 : 1000
            },
            .mode = modes[i]
        };
        esp_display_present_engine_t *p;
        OK(esp_display_present_engine_create(&cfg, &p));
        esp_display_present_backend_caps_t caps;
        OK(esp_display_present_engine_get_caps(p, &caps));
        assert(caps.retains_content == (i == 1 || i == 2));
        assert(!caps.supports_present_confirmation && caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
        destroy(p, &panel);
    }
}
/* Older SDK without safe timer join: explicit service remains usable; an
 * autonomous backend worker must fail creation rather than park forever. */
static void manual_without_phase_timer(void)
{
    sync_panel_t panel;
    sync_panel_init(&panel);
    esp_display_present_engine_config_t config = {.backend = {
            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = sync_panel_interface(&panel),
            .te_budget_us = 100000,
            .te_timing = {.period_us = 500000, .scan_us = 400000, .guard_us = 1000}
        }, .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE, .timeout_ms = 1000
    };
    unavailable_timer = true;
    esp_display_present_engine_t *presenter;
    OK(esp_display_present_engine_create(&config, &presenter));
    frame(presenter);
    uint64_t now = test_now_us();
    esp_display_present_engine_notify_te(presenter, now - 850000);
    esp_display_present_engine_notify_te(presenter, now - 350000);
    OK(esp_display_present_engine_service(presenter));
    assert(panel.panel.count == 1);
    sync_complete(&panel);
    OK(esp_display_present_engine_drain(presenter, 1000));
    OK(esp_display_present_engine_stop(presenter, 1000));
    OK(esp_display_present_engine_destroy(presenter));
    config.background_service = true;
    presenter = (void *)1;
    assert(esp_display_present_engine_create(&config, &presenter) == ESP_ERR_NOT_SUPPORTED && !presenter);
    unavailable_timer = false;
    sync_panel_deinit(&panel);
}

int main(void)
{
    manual_without_phase_timer();
    esp_display_present_engine_config_t invalid = {.backend_kind = (esp_display_present_engine_backend_t)99};
    esp_display_present_engine_t *p = (void *)1;
    assert(esp_display_present_engine_create(&invalid, &p) == ESP_ERR_INVALID_ARG && p == NULL);
    io_modes();
    pressure();
    rejection_and_fault();
    sync_panel_t s;
    sync_panel_init(&s);
    p = create(&s, 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, wrong_owner, p) == 0);
    assert(pthread_join(thread, NULL) == 0);
    s.panel.immediate = true;
    for (unsigned i = 0; i < 10; i++) {
        frame(p);
        esp_display_present_engine_notify_te(p, test_now_us());
        OK(esp_display_present_engine_service(p));
    }
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.completed_frames == 10 && !status.pending_frames);
    destroy(p, &s);
    puts("Presenter TE: bounded admission, notification thread, retry, fault and stop passed");
    return 0;
}
