/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "platform/lcd/present_lcd_io.h"
#include "core/present_engine.h"
#include "platform/lcd/present_lcd_fb_scanout.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#ifdef PRESENT_SMOKE_LVGL
#include "present_lvgl.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Compile/run smoke fixture, not an LCD adapter or a board configuration. */
typedef struct {
    esp_display_present_panel_event_fn done;
    void *user;
    bool scanout;
} loopback_t;

static esp_err_t attach(void *context, esp_display_present_panel_event_fn done, void *user)
{
    loopback_t *loop = context;
    loop->done = done;
    loop->user = user;
    return ESP_OK;
}

static esp_display_present_tx_result_t submit(void *context, const esp_display_present_surface_t *surface,
                                              const esp_display_present_rect_t *area, uint64_t cookie)
{
    loopback_t *loop = context;
    assert(surface->layout.width == area->width);
    const uint8_t *pixels = surface->pixels;
    assert(pixels[0] == 0x12 && pixels[1] == 0x34);
    const esp_display_present_backend_event_t event = {
        .kind = loop->scanout ? ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED : ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE,
        .transaction_cookie = cookie, .result = ESP_OK,
    };
    loop->done(loop->user, &event, false);
    return (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
}

static esp_err_t quiesce(void *context, uint32_t timeout)
{
    (void)timeout;
    loopback_t *loop = context;
    loop->done = NULL;
    loop->user = NULL;
    return ESP_OK;
}

static void async_fb_smoke(void)
{
    loopback_t loop = {.scanout = true};
    const esp_display_present_backend_config_t config = {
        .width = 2, .height = 2, .tile_rows = 2, .buffer_count = 2, .max_frames = 1,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .draw_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE, .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED,
        .panel = {&loop, attach, submit, quiesce}, .async_copy = true,
    };
    const present_backend_ops_t *ops = &present_fb_backend;
    present_backend_t *backend;
    ESP_ERROR_CHECK(ops->create(&config, &backend));
    for (unsigned iteration = 0; iteration < 16; ++iteration) {
        esp_display_present_frame_request_t request = {.full_redraw = true};
        esp_display_present_frame_t frame;
        ESP_ERROR_CHECK(ops->begin(backend, &request, &frame));
        esp_display_present_lease_t lease;
        int64_t deadline = esp_timer_get_time() + 1000000;
        esp_err_t acquired;
        while ((acquired = ops->acquire(backend, frame, NULL, 0, &lease)) == ESP_ERR_TIMEOUT &&
                esp_timer_get_time() < deadline) {
            vTaskDelay(1); /* Bare backend: pump/retry async prefill, bounded. */
        }
        ESP_ERROR_CHECK(acquired);
        const uint8_t pixels[8] = {0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde};
        memcpy(lease.surface.pixels, pixels, sizeof(pixels));
        ESP_ERROR_CHECK(ops->submit_region(backend, frame, lease.id));
        esp_display_present_receipt_t receipt;
        ESP_ERROR_CHECK(ops->end(backend, frame, &receipt));
        esp_display_present_frame_status_t status;
        ESP_ERROR_CHECK(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 1000, &status));
        ESP_ERROR_CHECK(status.result);
        assert(status.present_confirmed);
        ESP_ERROR_CHECK(ops->release_receipt(backend, receipt));
    }
    ESP_ERROR_CHECK(ops->stop(backend, 1000));
    ESP_ERROR_CHECK(ops->destroy(backend));
    puts("present_v2 async FB loopback passed (CPU worker, simulated scanout)");
}

void app_main(void)
{
    /* Force real esp_lcd adapter compilation/linkage without touching hardware. */
    esp_display_present_lcd_fb_scanout_t *scanout = NULL;
    assert(esp_display_present_lcd_fb_scanout_create(NULL, &scanout) == ESP_ERR_INVALID_ARG);
    esp_display_present_panel_adapter_t scanout_api;
    assert(esp_display_present_lcd_fb_scanout_get_interface(NULL, &scanout_api) == ESP_ERR_INVALID_ARG);
    esp_display_present_lcd_io_t *spi = NULL;
    assert(esp_display_present_lcd_io_create(NULL, &spi) == ESP_ERR_INVALID_ARG && spi == NULL);
    esp_display_present_panel_adapter_t spi_api;
    assert(esp_display_present_lcd_io_get_interface(NULL, &spi_api) == ESP_ERR_INVALID_ARG);
    present_backend_t *te = NULL;
    assert(present_te_backend.create(NULL, &te) == ESP_ERR_INVALID_ARG && te == NULL);
    esp_display_present_engine_t *presenter;
    assert(esp_display_present_engine_create(NULL, &presenter) == ESP_ERR_INVALID_ARG);
#ifdef PRESENT_SMOKE_LVGL
    esp_lv_present_native_t *binding;
    assert(esp_lv_present_native_create(NULL, 0, &binding) == ESP_ERR_INVALID_ARG);
#endif
    loopback_t loop = {0};
    const esp_display_present_backend_config_t config = {
        .width = 2, .height = 2, .tile_rows = 2, .buffer_count = 1, .max_frames = 1,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .draw_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE, .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED,
        .panel = { &loop, attach, submit, quiesce },
    };
    const present_backend_ops_t *ops = &present_region_backend;
    present_backend_t *backend;
    ESP_ERROR_CHECK(ops->create(&config, &backend));
    esp_display_present_frame_request_t request = { .full_redraw = true };
    esp_display_present_frame_t frame;
    ESP_ERROR_CHECK(ops->begin(backend, &request, &frame));
    esp_display_present_lease_t lease;
    const esp_display_present_rect_t area = { 0, 0, 2, 2 };
    ESP_ERROR_CHECK(ops->acquire(backend, frame, &area, 0, &lease));
    const uint8_t pixels[8] = { 0x34, 0x12, 0x78, 0x56, 0xBC, 0x9A, 0xF0, 0xDE };
    memcpy(lease.surface.pixels, pixels, sizeof(pixels));
    ESP_ERROR_CHECK(ops->submit_region(backend, frame, lease.id));
    esp_display_present_receipt_t receipt;
    ESP_ERROR_CHECK(ops->end(backend, frame, &receipt));
    esp_display_present_frame_status_t status;
    ESP_ERROR_CHECK(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status));
    ESP_ERROR_CHECK(status.result);
    ESP_ERROR_CHECK(ops->release_receipt(backend, receipt));
    ESP_ERROR_CHECK(ops->stop(backend, 0));
    ESP_ERROR_CHECK(ops->destroy(backend));
    async_fb_smoke();
    puts("present_v2 smoke passed (CPU conversion and loopback GRAM; no real LCD)");
}
