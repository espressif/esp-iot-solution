/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include <inttypes.h>
#include <string.h>
#include "board_qspi.h"
#include "esp_display_present.h"
#include "core/present_engine.h"
#include "platform/lcd/present_lcd_io.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static esp_display_present_engine_t *te_presenter;
static uint64_t last_rise, period_min = UINT64_MAX, period_max, high_min = UINT64_MAX;
static unsigned edges;
static const char *TAG = "qspi_test";
static void check(esp_err_t e, const char *what)
{
    while (e != ESP_OK) {
        ESP_LOGE(TAG, "FAIL %s: %s; objects retained", what, esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
static void te_isr(void *unused)
{
    (void)unused;
    uint64_t now = esp_timer_get_time();
    bool yield = false;
    portENTER_CRITICAL_ISR(&mux);
    if (gpio_get_level(QSPI_TE_GPIO)) {
        if (last_rise) {
            uint64_t delta = now - last_rise;
            if (delta < period_min) {
                period_min = delta;
            }
            if (delta > period_max) {
                period_max = delta;
            }
        }
        last_rise = now;
        edges++;
        if (te_presenter) {
            yield = esp_display_present_engine_notify_te_isr(te_presenter, now);
        }
    } else if (last_rise && now - last_rise < high_min) {
        high_min = now - last_rise;
    }
    portEXIT_CRITICAL_ISR(&mux);
    if (yield) {
        portYIELD_FROM_ISR();
    }
}

typedef struct {
    esp_display_present_panel_adapter_t real;
    esp_display_present_panel_event_fn callback;
    void *user;
    uint64_t started, completed, max_tx, min_edge_age, max_edge_age;
    unsigned offered, done, overruns;
    uint64_t edge_at_submit;
    uint32_t window;
} meter_t;
static bool metered_done(void *ctx, const esp_display_present_backend_event_t *e, bool isr)
{
    meter_t *m = ctx;
    uint64_t now = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&mux);
    m->completed = now;
    uint64_t us = now - m->started;
    if (us > m->max_tx) {
        m->max_tx = us;
    }
    if (m->window && now - m->edge_at_submit > m->window) {
        m->overruns++;
    }
    m->done++;
    portEXIT_CRITICAL_ISR(&mux);
    return m->callback(m->user, e, isr);
}
static esp_err_t metered_attach(void *ctx, esp_display_present_panel_event_fn callback, void *user)
{
    meter_t *m = ctx;
    m->callback = callback;
    m->user = user;
    return m->real.attach(m->real.context, metered_done, m);
}
static esp_display_present_tx_result_t metered_submit(void *ctx, const esp_display_present_surface_t *s, const esp_display_present_rect_t *area, uint64_t cookie)
{
    meter_t *m = ctx;
    portENTER_CRITICAL(&mux);
    uint64_t now = esp_timer_get_time();
    uint64_t rise = last_rise;
    m->started = now;
    m->edge_at_submit = rise;
    portEXIT_CRITICAL(&mux);
    esp_display_present_tx_result_t res = m->real.submit(m->real.context, s, area, cookie);
    if (res.acceptance != ESP_DISPLAY_PRESENT_TX_REJECTED) {
        portENTER_CRITICAL(&mux);
        uint64_t age = rise ? now - rise : 0;
        if (!m->offered || age < m->min_edge_age) {
            m->min_edge_age = age;
        }
        if (age > m->max_edge_age) {
            m->max_edge_age = age;
        }
        m->offered++;
        portEXIT_CRITICAL(&mux);
    }
    return res;
}
static esp_err_t metered_stop(void *ctx, uint32_t timeout)
{
    meter_t *m = ctx;
    return m->real.quiesce(m->real.context, timeout);
}
static const uint16_t colors[8] = {0xf800, 0x07e0, 0x001f, 0xffff, 0, 0xffe0, 0x07ff, 0xf81f};

/* Full-screen GRAM uses bands; TE uses a 64x16 centered ROI to respect internal
 * DMA storage and measure whole-ROI scheduling without pretending full-screen
 * 40MHz transfer fits one 60Hz refresh window. */
static uint64_t run(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                    bool te, bool roi, unsigned frames, uint32_t window, uint32_t budget)
{
    unsigned w = roi ? 64 : 480, h = roi ? 16 : 480, tile = 16;
    check(esp_lcd_panel_set_gap(panel, roi ? 208 : 0, roi ? 232 : 0), "gap");
    esp_display_present_lcd_io_t *adapter;
    esp_display_present_lcd_io_config_t spi = {.panel = panel, .io = io, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .max_inflight = 1, .psram_dma_direct = true};
    check(esp_display_present_lcd_io_create(&spi, &adapter), "adapter create");
    meter_t meter = {.window = te ? window : 0};
    check(esp_display_present_lcd_io_get_interface(adapter, &meter.real), "adapter interface");
    esp_display_present_engine_config_t cfg = {.backend_kind = te ? ESP_DISPLAY_PRESENT_ENGINE_TE : ESP_DISPLAY_PRESENT_ENGINE_REGION, .timeout_ms = 2000,
                                               .backend = {.image_in_psram = te && !roi, .compose_count = te ? 2 : 0, .width = w, .height = h, .tile_rows = tile, .buffer_count = 2, .max_frames = 3,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED,
                                                           .panel = {&meter, metered_attach, metered_submit, metered_stop},
                                                           .te_window_us = window, .te_budget_us = budget
                                                          }
                                              };
    esp_display_present_engine_t *p;
    check(esp_display_present_engine_create(&cfg, &p), "presenter create");
    if (te) {
        portENTER_CRITICAL(&mux);
        te_presenter = p;
        portEXIT_CRITICAL(&mux);
    }
    ESP_LOGI(TAG, "BEGIN %s %ux%u frames=%u window=%"PRIu32" budget=%"PRIu32,
             te ? (roi ? "TE_ROI" : "TE_FULL_EDGE_SYNC_UNVERIFIED") : roi ? "GRAM_ROI" : "GRAM_FULL", w, h, frames, window, budget);
    esp_display_present_engine_receipt_t batch[2];
    unsigned batch_count = 0;
    for (unsigned f = 0; f < frames; f++) {
        esp_display_present_frame_request_t req = {.full_redraw = true};
        check(esp_display_present_engine_begin_next_frame(p, &req), "begin");
        for (unsigned y = 0; y < h; y += tile) {
            esp_display_present_lease_t l;
            check(esp_display_present_engine_acquire_buffer(p, &l), "acquire");
            uint16_t *pixels = l.surface.pixels;
            unsigned rows = h - y < tile ? h - y : tile;
            for (unsigned j = 0; j < rows; j++) {
                for (unsigned x = 0; x < w; x++) {
                    uint16_t c = colors[x * 8 / w];
                    if (roi && (x >= (f * 3) % (w - 12) && x < (f * 3) % (w - 12) + 12)) {
                        c = 0xffff;
                    }
                    if (x < 2 || x >= w - 2 || y + j < 2 || y + j >= h - 2) {
                        c = te ? 0x07e0 : 0xffff;
                    }
                    pixels[j * w + x] = c;
                }
            }
            esp_display_present_engine_area_t area = {0, y, w - 1, y + rows - 1};
            check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "submit");
        }
        check(esp_display_present_engine_commit_with_receipt(p, NULL, &batch[batch_count++]), "commit");
        if (te && batch_count < 2 && f + 1 < frames) {
            continue;
        }
        for (unsigned i = 0; i < batch_count; i++) {
            esp_display_present_frame_status_t state;
            check(esp_display_present_engine_wait_frame(p, batch[i], ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 2000, &state), "wait");
            check(state.result, "result");
            check(state.terminal && state.src_released ? ESP_OK : ESP_FAIL, "terminal");
            check(esp_display_present_engine_release_receipt(p, batch[i]), "release");
        }
        batch_count = 0;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    portENTER_CRITICAL(&mux);
    te_presenter = NULL;
    portEXIT_CRITICAL(&mux);
    check(esp_display_present_engine_stop(p, 2000), "stop");
    check(esp_display_present_engine_destroy(p), "destroy");
    check(esp_display_present_lcd_io_destroy(adapter), "adapter destroy");
    check(meter.done == meter.offered ? ESP_OK : ESP_FAIL, "callbacks balanced");
    ESP_LOGI(TAG, "window_overruns=%u", meter.overruns);
    check(meter.overruns == 0 ? ESP_OK : ESP_FAIL, "TE pulse overrun");
    ESP_LOGI(TAG, "PASS %s frames=%u tx=%u callbacks=%u max_tx_us=%"PRIu64" max_edge_age_us=%"PRIu64" internal=%u psram=%u",
             te ? (roi ? "TE_ROI" : "TE_FULL_EDGE_SYNC_UNVERIFIED") : roi ? "GRAM_ROI" : "GRAM_FULL", frames, meter.offered, meter.done, meter.max_tx, meter.max_edge_age,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return meter.max_tx;
}

static void run_rotation_async(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                               bool te, size_t compose_count, uint32_t window, uint32_t budget)
{
    unsigned w = 480, h = 480, tile = 16;
    check(esp_lcd_panel_set_gap(panel, 0, 0), "gap");
    esp_display_present_lcd_io_t *adapter;
    esp_display_present_lcd_io_config_t spi = {.panel = panel, .io = io, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .max_inflight = 1, .psram_dma_direct = true};
    check(esp_display_present_lcd_io_create(&spi, &adapter), "adapter create");
    meter_t meter = {.window = te ? window : 0};
    check(esp_display_present_lcd_io_get_interface(adapter, &meter.real), "adapter interface");
    esp_display_present_engine_config_t cfg = {.backend_kind = te ? ESP_DISPLAY_PRESENT_ENGINE_TE : ESP_DISPLAY_PRESENT_ENGINE_REGION,
                                               .timeout_ms = 2000, .background_service = true,
                                               .backend = {.image_in_psram = te, .compose_count = te ? compose_count : 0,
                                                           .width = w, .height = h, .tile_rows = tile, .buffer_count = 2, .max_frames = 3,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .async_copy = true,
                                                           .panel = {&meter, metered_attach, metered_submit, metered_stop},
                                                           .te_window_us = window, .te_budget_us = budget
                                                          }
                                              };
    esp_display_present_engine_t *p;
    check(esp_display_present_engine_create(&cfg, &p), "presenter create");
    if (te) {
        portENTER_CRITICAL(&mux);
        te_presenter = p;
        portEXIT_CRITICAL(&mux);
    }
    const esp_display_present_engine_rotation_t rots[] = {ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_180, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0};
    for (size_t r = 0; r < sizeof(rots) / sizeof(rots[0]); ++r) {
        check(esp_display_present_engine_set_rotation(p, rots[r], 2000), "set_rotation");
        for (unsigned f = 0; f < 2; ++f) {
            esp_display_present_frame_request_t req = {.full_redraw = true};
            check(esp_display_present_engine_begin_next_frame(p, &req), "begin");
            for (unsigned y = 0; y < h; y += tile) {
                esp_display_present_lease_t l;
                check(esp_display_present_engine_acquire_buffer(p, &l), "acquire");
                uint16_t *pixels = l.surface.pixels;
                unsigned rows = h - y < tile ? h - y : tile;
                for (unsigned j = 0; j < rows; j++) {
                    for (unsigned x = 0; x < w; x++) {
                        uint16_t c = colors[x * 8 / w];
                        if (x < 2 || x >= w - 2 || y + j < 2 || y + j >= h - 2) {
                            c = te ? 0x07e0 : 0xffff;
                        }
                        pixels[j * w + x] = c;
                    }
                }
                esp_display_present_engine_area_t area = {0, y, w - 1, y + rows - 1};
                check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "submit");
            }
            esp_display_present_engine_receipt_t receipt;
            check(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt), "commit");
            esp_display_present_frame_status_t state;
            check(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 2000, &state), "wait");
            check(state.result, "result");
            check(esp_display_present_engine_release_receipt(p, receipt), "release");
        }
    }
    portENTER_CRITICAL(&mux);
    te_presenter = NULL;
    portEXIT_CRITICAL(&mux);
    check(esp_display_present_engine_stop(p, 2000), "stop");
    check(esp_display_present_engine_destroy(p), "destroy");
    check(esp_display_present_lcd_io_destroy(adapter), "adapter destroy");
    check(meter.done == meter.offered ? ESP_OK : ESP_FAIL, "callbacks balanced");
    check(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL, "heap integrity");
    ESP_LOGI(TAG, "COMBO_PASS kind=%s compose=%u rots=5 tx=%u max_tx_us=%"PRIu64" min_int=%u min_psram=%u free_int=%u free_psram=%u",
             te ? "TE_ASYNC_ROT" : "GRAM_ASYNC_ROT", (unsigned)compose_count, meter.offered, meter.max_tx,
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void run_dynamic_phase_te(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                                 uint64_t period_min_us, uint64_t period_max_us,
                                 uint64_t high_us, uint64_t measured_full_tx_us)
{
    unsigned w = 480, h = 480, tile = 16, frames = 24;
    check(esp_lcd_panel_set_gap(panel, 0, 0), "gap");
    uint64_t period_avg = (period_min_us + period_max_us) / 2;
    uint64_t jitter = (period_max_us - period_min_us) + 250;
    uint64_t guard = jitter + 400;
    uint64_t edge_to_scan = high_us < period_avg / 4 ? high_us : 536;
    uint64_t scan_us = period_avg - edge_to_scan;
    uint32_t budget_us = (uint32_t)(measured_full_tx_us + 1200);
    esp_display_present_te_timing_t timing = {
        .period_us = period_avg,
        .scan_us = scan_us,
        .edge_to_scan_us = edge_to_scan,
        .guard_us = guard,
        .jitter_us = jitter,
    };
    esp_display_present_te_window_t win = esp_display_present_te_window(&timing, budget_us, 100000, 100000 + edge_to_scan + guard, true);
    ESP_LOGI(TAG, "TE_DYNAMIC_MODEL period=%"PRIu64" scan=%"PRIu64" edge_to_scan=%"PRIu64" guard=%"PRIu64" jitter=%"PRIu64" budget=%"PRIu32" window=[%"PRIu64", %"PRIu64") admission=%d",
             timing.period_us, timing.scan_us, timing.edge_to_scan_us, timing.guard_us, timing.jitter_us,
             budget_us, win.earliest_phase_us, win.latest_phase_us, (int)win.admission);
    check(win.admission == ESP_DISPLAY_PRESENT_TE_ALLOW ? ESP_OK : ESP_ERR_NOT_SUPPORTED, "dynamic TE window admission");

    esp_display_present_lcd_io_t *adapter;
    esp_display_present_lcd_io_config_t spi = {.panel = panel, .io = io, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .max_inflight = 1, .psram_dma_direct = true};
    check(esp_display_present_lcd_io_create(&spi, &adapter), "adapter create");
    meter_t meter = {0};
    check(esp_display_present_lcd_io_get_interface(adapter, &meter.real), "adapter interface");
    esp_display_present_engine_config_t cfg = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE, .timeout_ms = 2000, .background_service = true,
                                               .backend = {.image_in_psram = true, .compose_count = 2,
                                                           .width = w, .height = h, .tile_rows = tile, .buffer_count = 2, .max_frames = 3,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .async_copy = true,
                                                           .panel = {&meter, metered_attach, metered_submit, metered_stop},
                                                           .te_timing = timing, .te_budget_us = budget_us
                                                          }
                                              };
    esp_display_present_engine_t *p;
    check(esp_display_present_engine_create(&cfg, &p), "dynamic TE presenter create");
    portENTER_CRITICAL(&mux);
    te_presenter = p;
    portEXIT_CRITICAL(&mux);
    /* Let two TE edges establish measured=true on the backend before frame 0. */
    vTaskDelay(pdMS_TO_TICKS(50));
    for (unsigned f = 0; f < frames; f++) {
        esp_display_present_frame_request_t req = {.full_redraw = true};
        check(esp_display_present_engine_begin_next_frame(p, &req), "begin");
        for (unsigned y = 0; y < h; y += tile) {
            esp_display_present_lease_t l;
            check(esp_display_present_engine_acquire_buffer(p, &l), "acquire");
            uint16_t *pixels = l.surface.pixels;
            unsigned rows = h - y < tile ? h - y : tile;
            for (unsigned j = 0; j < rows; j++) {
                for (unsigned x = 0; x < w; x++) {
                    uint16_t c = colors[x * 8 / w];
                    if (x >= (f * 16) % (w - 48) && x < (f * 16) % (w - 48) + 48) {
                        c = 0xffff;
                    }
                    if (x < 2 || x >= w - 2 || y + j < 2 || y + j >= h - 2) {
                        c = 0x07e0;
                    }
                    pixels[j * w + x] = c;
                }
            }
            esp_display_present_engine_area_t area = {0, y, w - 1, y + rows - 1};
            check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "submit");
        }
        esp_display_present_engine_receipt_t receipt;
        check(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt), "commit");
        esp_display_present_frame_status_t state;
        check(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 2000, &state), "wait");
        check(state.result, "dynamic TE frame result");
        check(state.terminal && state.src_released ? ESP_OK : ESP_FAIL, "dynamic TE terminal");
        check(esp_display_present_engine_release_receipt(p, receipt), "release");
    }
    portENTER_CRITICAL(&mux);
    te_presenter = NULL;
    portEXIT_CRITICAL(&mux);
    check(esp_display_present_engine_stop(p, 2000), "stop");
    check(esp_display_present_engine_destroy(p), "destroy");
    check(esp_display_present_lcd_io_destroy(adapter), "adapter destroy");
    check(meter.done == frames && meter.offered == frames ? ESP_OK : ESP_FAIL, "dynamic TE callbacks");
    check(meter.max_tx <= budget_us ? ESP_OK : ESP_FAIL, "dynamic TE transfer within budget");
    uint64_t min_phase = meter.min_edge_age >= timing.edge_to_scan_us ? meter.min_edge_age - timing.edge_to_scan_us : 0;
    uint64_t max_phase = meter.max_edge_age >= timing.edge_to_scan_us ? meter.max_edge_age - timing.edge_to_scan_us : 0;
    ESP_LOGI(TAG, "PASS TE_FULL_DYNAMIC_PHASE frames=%u tx=%u callbacks=%u max_tx_us=%"PRIu64" budget_us=%"PRIu32" phase_us=[%"PRIu64", %"PRIu64"] window=[%"PRIu64", %"PRIu64")",
             frames, meter.offered, meter.done, meter.max_tx, budget_us, min_phase, max_phase,
             win.earliest_phase_us, win.latest_phase_us);
    check(min_phase >= win.earliest_phase_us && max_phase < win.latest_phase_us ? ESP_OK : ESP_FAIL, "dynamic TE phase bounds");
}

static void run_te_matrix_and_cancel_profile(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                                             uint64_t period_min_us, uint64_t period_max_us,
                                             uint64_t high_us, uint64_t measured_full_tx_us)
{
    unsigned w = 480, h = 480, tile = 16;
    check(esp_lcd_panel_set_gap(panel, 0, 0), "gap");
    uint64_t period_avg = (period_min_us + period_max_us) / 2;
    uint64_t jitter = (period_max_us - period_min_us) + 250;
    uint64_t guard = jitter + 400;
    uint64_t edge_to_scan = high_us < period_avg / 4 ? high_us : 536;
    uint64_t scan_us = period_avg - edge_to_scan;
    uint32_t budget_us = (uint32_t)(measured_full_tx_us + 1200);
    esp_display_present_te_timing_t timing = {
        .period_us = period_avg, .scan_us = scan_us, .edge_to_scan_us = edge_to_scan,
        .guard_us = guard, .jitter_us = jitter,
    };

    for (size_t compose_count = 1; compose_count <= 2; ++compose_count) {
        for (size_t tile_count = 1; tile_count <= 2; ++tile_count) {
            esp_display_present_lcd_io_t *adapter;
            esp_display_present_lcd_io_config_t spi = {.panel = panel, .io = io, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                       .byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .max_inflight = 1, .psram_dma_direct = true
                                                      };
            check(esp_display_present_lcd_io_create(&spi, &adapter), "matrix adapter create");
            meter_t meter = {0};
            check(esp_display_present_lcd_io_get_interface(adapter, &meter.real), "matrix adapter interface");
            esp_display_present_engine_config_t cfg = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_TE, .timeout_ms = 2000, .background_service = true,
                                                       .backend = {.image_in_psram = true, .compose_count = compose_count,
                                                                   .width = w, .height = h, .tile_rows = tile, .buffer_count = tile_count,
                                                                   .max_frames = 3, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                                   .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .async_copy = true,
                                                                   .panel = {&meter, metered_attach, metered_submit, metered_stop},
                                                                   .te_timing = timing, .te_budget_us = budget_us
                                                                  }
                                                      };
            esp_display_present_engine_t *p;
            check(esp_display_present_engine_create(&cfg, &p), "matrix presenter create");
            portENTER_CRITICAL(&mux);
            te_presenter = p;
            portEXIT_CRITICAL(&mux);
            vTaskDelay(pdMS_TO_TICKS(40));

            uint64_t sum_tile_wait = 0, max_tile_wait = 0;
            uint64_t sum_draw_sub = 0, max_draw_sub = 0;
            uint64_t sum_te_idle = 0, max_te_idle = 0, min_te_idle = UINT64_MAX;
            uint64_t sum_dma_tx = 0, max_dma_tx = 0;
            unsigned normal_frames = 6;

            for (unsigned f = 0; f < normal_frames; ++f) {
                esp_display_present_frame_request_t req = {.full_redraw = f == 0};
                check(esp_display_present_engine_begin_next_frame(p, &req), "matrix begin");
                uint64_t f_tile_wait = 0, f_draw_sub = 0;
                /* Frame 0 is full redraw (30 bands); subsequent frames update 4 dirty bands (64 rows) to exercise repair. */
                unsigned bands = (f == 0) ? (h / tile) : 4;
                for (unsigned b_idx = 0; b_idx < bands; ++b_idx) {
                    unsigned y = (f == 0) ? (b_idx * tile) : ((f * 32 + b_idx * tile) % (h - tile));
                    esp_display_present_lease_t l;
                    uint64_t t0 = esp_timer_get_time();
                    check(esp_display_present_engine_acquire_buffer(p, &l), "matrix acquire");
                    uint64_t t1 = esp_timer_get_time();
                    f_tile_wait += (t1 - t0);
                    uint16_t *pixels = l.surface.pixels;
                    for (unsigned j = 0; j < tile; ++j) {
                        for (unsigned x = 0; x < w; ++x) {
                            pixels[j * w + x] = colors[(x * 8 / w + f) % 8];
                        }
                    }
                    esp_display_present_engine_area_t area = {0, y, w - 1, y + tile - 1};
                    check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "matrix submit");
                    uint64_t t2 = esp_timer_get_time();
                    f_draw_sub += (t2 - t1);
                }
                sum_tile_wait += f_tile_wait;
                if (f_tile_wait > max_tile_wait) {
                    max_tile_wait = f_tile_wait;
                }
                sum_draw_sub += f_draw_sub;
                if (f_draw_sub > max_draw_sub) {
                    max_draw_sub = f_draw_sub;
                }
                esp_display_present_engine_receipt_t receipt;
                uint64_t t_commit = esp_timer_get_time();
                check(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt), "matrix commit");
                esp_display_present_frame_status_t st;
                check(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 2000, &st), "matrix wait");
                check(st.result, "matrix frame result");
                portENTER_CRITICAL(&mux);
                uint64_t tx_start = meter.started;
                uint64_t tx_end = meter.completed;
                portEXIT_CRITICAL(&mux);
                uint64_t te_idle = tx_start > t_commit ? (tx_start - t_commit) : 0;
                uint64_t dma_tx = tx_end > tx_start ? (tx_end - tx_start) : 0;
                sum_te_idle += te_idle;
                if (te_idle > max_te_idle) {
                    max_te_idle = te_idle;
                }
                if (te_idle < min_te_idle) {
                    min_te_idle = te_idle;
                }
                sum_dma_tx += dma_tx;
                if (dma_tx > max_dma_tx) {
                    max_dma_tx = dma_tx;
                }
                check(esp_display_present_engine_release_receipt(p, receipt), "matrix release");
            }

            /* Exercise mid-frame cancel while async tile copy is in flight, then verify baseline semantics. */
            esp_display_present_frame_request_t creq = {.full_redraw = false};
            check(esp_display_present_engine_begin_next_frame(p, &creq), "cancel begin");
            for (unsigned b_idx = 0; b_idx < 10; ++b_idx) {
                esp_display_present_lease_t l;
                check(esp_display_present_engine_acquire_buffer(p, &l), "cancel acquire");
                memset(l.surface.pixels, 0xaa, tile * w * 2);
                esp_display_present_engine_area_t area = {0, b_idx * tile, w - 1, (b_idx + 1) * tile - 1};
                check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "cancel submit");
            }
            uint64_t tc0 = esp_timer_get_time();
            check(esp_display_present_engine_cancel_frame(p), "cancel_frame");
            uint64_t cancel_drain_us = esp_timer_get_time() - tc0;

            /* Partial frame right after cancel: compose=1 must reject, compose=2 must repair & succeed. */
            check(esp_display_present_engine_begin_next_frame(p, &creq), "post-cancel partial begin");
            {
                esp_display_present_lease_t l;
                check(esp_display_present_engine_acquire_buffer(p, &l), "post-cancel partial acquire");
                memset(l.surface.pixels, 0x55, tile * w * 2);
                esp_display_present_engine_area_t area = {0, 0, w - 1, tile - 1};
                check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "post-cancel partial submit");
            }
            esp_display_present_engine_receipt_t post_r;
            esp_err_t post_err = esp_display_present_engine_commit_with_receipt(p, NULL, &post_r);
            if (compose_count == 1) {
                check(post_err == ESP_ERR_INVALID_STATE ? ESP_OK : ESP_FAIL, "compose=1 must reject partial after cancel");
                check(esp_display_present_engine_cancel_frame(p), "compose=1 cancel invalid partial");
                /* Full redraw recovers compose=1 baseline. */
                esp_display_present_frame_request_t freq = {.full_redraw = true};
                check(esp_display_present_engine_begin_next_frame(p, &freq), "compose=1 recovery begin");
                for (unsigned y = 0; y < h; y += tile) {
                    esp_display_present_lease_t l;
                    check(esp_display_present_engine_acquire_buffer(p, &l), "compose=1 recovery acquire");
                    memset(l.surface.pixels, 0x33, tile * w * 2);
                    esp_display_present_engine_area_t area = {0, y, w - 1, y + tile - 1};
                    check(esp_display_present_engine_submit_buffer(p, &l, &area, w * 2), "compose=1 recovery submit");
                }
                check(esp_display_present_engine_commit_with_receipt(p, NULL, &post_r), "compose=1 recovery commit");
            } else {
                check(post_err, "compose=2 partial commit after cancel");
            }
            esp_display_present_frame_status_t post_st;
            check(esp_display_present_engine_wait_frame(p, post_r, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 2000, &post_st), "post-cancel wait");
            check(post_st.result, "post-cancel result");
            check(esp_display_present_engine_release_receipt(p, post_r), "post-cancel release");

            portENTER_CRITICAL(&mux);
            te_presenter = NULL;
            portEXIT_CRITICAL(&mux);
            check(esp_display_present_engine_stop(p, 2000), "matrix stop");
            check(esp_display_present_engine_destroy(p), "matrix destroy");
            check(esp_display_present_lcd_io_destroy(adapter), "matrix adapter destroy");

            ESP_LOGI(TAG, "TE_MATRIX_PASS compose=%u tiles=%u avg_tile_wait_us=%"PRIu64" max_tile_wait_us=%"PRIu64" avg_draw_sub_us=%"PRIu64" max_draw_sub_us=%"PRIu64" te_idle_wait_us=[%"PRIu64", avg=%"PRIu64", %"PRIu64"] avg_dma_tx_us=%"PRIu64" max_dma_tx_us=%"PRIu64" cancel_drain_us=%"PRIu64,
                     (unsigned)compose_count, (unsigned)tile_count,
                     sum_tile_wait / normal_frames, max_tile_wait,
                     sum_draw_sub / normal_frames, max_draw_sub,
                     min_te_idle, sum_te_idle / normal_frames, max_te_idle,
                     sum_dma_tx / normal_frames, max_dma_tx, cancel_drain_us);
        }
    }
}

static void run_v1_vs_v2_qspi_bench(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                                    uint64_t period_min_us, uint64_t period_max_us,
                                    uint64_t high_us, uint64_t measured_full_tx_us)
{
    const unsigned w = 480, h = 480, tile = 16, warmup_frames = 4, bench_frames = 12, total_run = warmup_frames + bench_frames;
    check(esp_lcd_panel_set_gap(panel, 0, 0), "gap");
    uint64_t period_avg = (period_min_us + period_max_us) / 2;
    uint64_t jitter = (period_max_us - period_min_us) + 120;
    uint64_t guard = jitter;
    uint64_t edge_to_scan = high_us < period_avg / 4 ? high_us : 536;
    uint64_t scan_us = period_avg - edge_to_scan;
    uint32_t budget_us = (uint32_t)(measured_full_tx_us + 250);
    esp_display_present_te_timing_t timing = {
        .period_us = period_avg, .scan_us = scan_us, .edge_to_scan_us = edge_to_scan,
        .guard_us = guard, .jitter_us = jitter,
    };

    static const struct {
        const char *name;
        bool te;
        uint8_t compose_buffers;
        bool partial_dirty;
    } qcases[] = {
        {"GRAM_FULL_480x480", false, 0, false},
        {"TE_COMPOSE1_PARTIAL", true, 1, true},
        {"TE_COMPOSE2_PARTIAL", true, 2, true},
        {"TE_COMPOSE2_FULL", true, 2, false},
    };

    for (size_t i = 0; i < sizeof(qcases) / sizeof(qcases[0]); ++i) {
        /* Release our TE GPIO ISR while V1 owns QSPI_TE_GPIO. */
        if (qcases[i].te) {
            check(gpio_isr_handler_remove(QSPI_TE_GPIO), "remove te isr for v1");
        }
        esp_display_presenter_config_t v1_cfg = {
            .width = w,
            .height = h,
            .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .max_damage_areas = 8,
            .transfer_timeout_ms = 2000,
            .pipeline_acquire_timeout_ms = 2000,
            .target = {
                .hw = {
                    .panel = panel,
                    .io = io,
                    .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
                    .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
                    .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
                    .swap_bytes = true,
                    .te_enabled = qcases[i].te,
                    .te_sync = qcases[i].te ? (esp_display_present_te_sync_config_t)
                    {
                        .gpio_num = QSPI_TE_GPIO,
                        .bus_freq_hz = 40000000,
                        .data_lines = 4,
} : ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED(),
                },
                .fb = {
                    .mode = qcases[i].te ? ESP_DISPLAY_PRESENT_MODE_TE_SYNC : ESP_DISPLAY_PRESENT_MODE_NONE,
                },
                .drawbuf = {
                    .lines = tile,
                    .buffers = 2,
                    .in_psram = false,
                    .te_compose_buffers = qcases[i].compose_buffers,
                },
            },
        };
        esp_display_presenter_t *v1 = NULL;
        check(esp_display_presenter_create(&v1_cfg, &v1), "qspi v1 create");
        if (qcases[i].te) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        size_t v1_int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t v1_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        uint64_t v1_t0 = 0;
        for (unsigned f = 0; f < total_run; ++f) {
            if (f == warmup_frames) {
                v1_t0 = esp_timer_get_time();
            }
            bool full = (f == 0) || !qcases[i].partial_dirty;
            int bx0 = 8 + ((f ? f - 1 : 0) * 16) % (w - 80);
            int by0 = 8 + ((f ? f - 1 : 0) * 16) % (h - 80);
            int bx1 = 8 + (f * 16) % (w - 80);
            int by1 = 8 + (f * 16) % (h - 80);
            esp_display_present_area_t dirty_v1[2] = {
                {bx0, by0, bx0 + 63, by0 + 63},
                {bx1, by1, bx1 + 63, by1 + 63},
            };
            esp_display_present_surface_request_t req_v1 = {
                .dirty_areas = full ? NULL : dirty_v1,
                .dirty_area_count = full ? 0 : 2,
            };
            esp_display_present_area_t render_areas[8];
            size_t render_count = 0;
            bool full_cov = false;
            check(esp_display_presenter_begin_next_frame(v1, &req_v1, render_areas, 8, &render_count, &full_cov), "qspi v1 begin");
            size_t n_areas = full ? 1 : 2;
            esp_display_present_area_t full_a = {0, 0, (int)w - 1, (int)h - 1};
            const esp_display_present_area_t *alist = full ? &full_a : dirty_v1;
            for (size_t a = 0; a < n_areas; ++a) {
                int h_total = alist[a].y2 - alist[a].y1 + 1;
                int w_total = alist[a].x2 - alist[a].x1 + 1;
                for (int off = 0; off < h_total; off += (int)tile) {
                    int rows = (h_total - off > (int)tile) ? (int)tile : (h_total - off);
                    esp_display_presenter_buffer_t buf;
                    check(esp_display_presenter_acquire_buffer(v1, &buf), "qspi v1 acquire");
                    memset(buf.surface.pixels, (int)(f + a), (size_t)w_total * rows * 2);
                    esp_display_present_area_t sub_a = {alist[a].x1, alist[a].y1 + off, alist[a].x2, alist[a].y1 + off + rows - 1};
                    check(esp_display_presenter_submit_buffer(v1, &buf, &sub_a, (size_t)w_total * 2), "qspi v1 submit");
                }
            }
            esp_display_presenter_submit_t sub = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL};
            check(esp_display_presenter_commit_frame(v1, &sub), "qspi v1 commit");
        }
        uint64_t v1_elapsed_us = esp_timer_get_time() - v1_t0;
        check(esp_display_presenter_quiesce(v1, 5000), "qspi v1 quiesce");
        check(esp_display_presenter_stop(v1), "qspi v1 stop");
        check(esp_display_presenter_delete(v1), "qspi v1 delete");
        if (qcases[i].te) {
            gpio_config_t pin = {.pin_bit_mask = 1ULL << QSPI_TE_GPIO, .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_ANYEDGE};
            check(gpio_config(&pin), "restore TE gpio");
            check(gpio_isr_handler_add(QSPI_TE_GPIO, te_isr, NULL), "restore te isr");
        }

        /* --- Run V2 with identical workload --- */
        esp_display_present_lcd_io_t *adapter;
        esp_display_present_lcd_io_config_t spi = {.panel = panel, .io = io, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                   .byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED, .max_inflight = qcases[i].te ? 1 : 2, .psram_dma_direct = true
                                                  };
        check(esp_display_present_lcd_io_create(&spi, &adapter), "qspi v2 adapter");
        meter_t meter = {0};
        check(esp_display_present_lcd_io_get_interface(adapter, &meter.real), "qspi v2 iface");
        esp_display_present_engine_config_t v2_cfg = {
            .backend_kind = qcases[i].te ? ESP_DISPLAY_PRESENT_ENGINE_TE : ESP_DISPLAY_PRESENT_ENGINE_REGION,
            .timeout_ms = 2000, .background_service = qcases[i].te,
            .backend = {
                .image_in_psram = qcases[i].te, .compose_count = qcases[i].compose_buffers,
                .width = w, .height = h, .tile_rows = tile, .buffer_count = 2, .max_frames = 3,
                .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED,
                .async_copy = qcases[i].te, .in_place = !qcases[i].te,
                .panel = {&meter, metered_attach, metered_submit, metered_stop},
                .te_timing = timing, .te_budget_us = budget_us,
            },
        };
        esp_display_present_engine_t *v2 = NULL;
        check(esp_display_present_engine_create(&v2_cfg, &v2), "qspi v2 create");
        if (qcases[i].te) {
            portENTER_CRITICAL(&mux);
            te_presenter = v2;
            portEXIT_CRITICAL(&mux);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        size_t v2_int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t v2_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        uint64_t v2_t0 = 0;
        for (unsigned f = 0; f < total_run; ++f) {
            if (f == warmup_frames) {
                v2_t0 = esp_timer_get_time();
            }
            bool full = (f == 0) || !qcases[i].partial_dirty;
            uint32_t bx0 = 8 + ((f ? f - 1 : 0) * 16) % (w - 80);
            uint32_t by0 = 8 + ((f ? f - 1 : 0) * 16) % (h - 80);
            uint32_t bx1 = 8 + (f * 16) % (w - 80);
            uint32_t by1 = 8 + (f * 16) % (h - 80);
            esp_display_present_rect_t dirty_v2[2] = {{bx0, by0, 64, 64}, {bx1, by1, 64, 64}};
            esp_display_present_frame_request_t req_v2 = {.full_redraw = full, .damage = full ? NULL : dirty_v2, .damage_count = full ? 0 : 2};
            check(esp_display_present_engine_begin_next_frame(v2, &req_v2), "qspi v2 begin");
            size_t n_areas = full ? 1 : 2;
            esp_display_present_rect_t full_r = {0, 0, w, h};
            const esp_display_present_rect_t *rlist = full ? &full_r : dirty_v2;
            for (size_t a = 0; a < n_areas; ++a) {
                for (uint32_t off = 0; off < rlist[a].height; off += tile) {
                    uint32_t rows = (rlist[a].height - off > tile) ? tile : (rlist[a].height - off);
                    esp_display_present_lease_t l;
                    check(esp_display_present_engine_acquire_buffer(v2, &l), "qspi v2 acquire");
                    memset(l.surface.pixels, (int)(f + a), (size_t)rlist[a].width * rows * 2);
                    esp_display_present_engine_area_t sub_a = {(int32_t)rlist[a].x, (int32_t)(rlist[a].y + off),
                                                               (int32_t)(rlist[a].x + rlist[a].width - 1), (int32_t)(rlist[a].y + off + rows - 1)
                                                              };
                    check(esp_display_present_engine_submit_buffer(v2, &l, &sub_a, (size_t)rlist[a].width * 2), "qspi v2 submit");
                }
            }
            check(esp_display_present_engine_commit_frame(v2, NULL), "qspi v2 commit");
        }
        uint64_t v2_elapsed_us = esp_timer_get_time() - v2_t0;
        check(esp_display_present_engine_drain(v2, 5000), "qspi v2 drain");
        portENTER_CRITICAL(&mux);
        te_presenter = NULL;
        portEXIT_CRITICAL(&mux);
        check(esp_display_present_engine_stop(v2, 2000), "qspi v2 stop");
        check(esp_display_present_engine_destroy(v2), "qspi v2 destroy");
        check(esp_display_present_lcd_io_destroy(adapter), "qspi v2 adapter destroy");

        uint32_t v1_fps_x10 = (uint32_t)((uint64_t)bench_frames * 10000000ULL / v1_elapsed_us);
        uint32_t v2_fps_x10 = (uint32_t)((uint64_t)bench_frames * 10000000ULL / v2_elapsed_us);
        ESP_LOGI(TAG, "BENCH_V1_V2 case=%s frames=%u v1_us_per_frame=%"PRIu64" v1_fps=%u.%u v2_us_per_frame=%"PRIu64" v2_fps=%u.%u v1_free=(%u,%u) v2_free=(%u,%u)",
                 qcases[i].name, bench_frames,
                 v1_elapsed_us / bench_frames, (unsigned)(v1_fps_x10 / 10), (unsigned)(v1_fps_x10 % 10),
                 v2_elapsed_us / bench_frames, (unsigned)(v2_fps_x10 / 10), (unsigned)(v2_fps_x10 % 10),
                 (unsigned)v1_int_free, (unsigned)v1_psram_free,
                 (unsigned)v2_int_free, (unsigned)v2_psram_free);
    }
}
void app_main(void)
{
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    check(qspi_board_create(&panel, &io), "board init");
    gpio_config_t pin = {.pin_bit_mask = 1ULL << QSPI_TE_GPIO, .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_ANYEDGE};
    check(gpio_config(&pin), "TE gpio");
    check(gpio_install_isr_service(0), "ISR service");
    check(gpio_isr_handler_add(QSPI_TE_GPIO, te_isr, NULL), "ISR handler");
    vTaskDelay(pdMS_TO_TICKS(1000));
    portENTER_CRITICAL(&mux);
    uint64_t min = period_min, max = period_max, high = high_min;
    unsigned n = edges;
    portEXIT_CRITICAL(&mux);
    ESP_LOGI(TAG, "TE measured edges=%u period_min=%"PRIu64" period_max=%"PRIu64" high_min=%"PRIu64, n, min, max, high);
    check(n > 10 && min > 1000 && min < 100000 ? ESP_OK : ESP_FAIL, "TE edges");
    run(panel, io, false, false, 8, 0, 0);
    /* Exercise real full-image ownership and DMA. This broad, explicit gate
     * only aligns with an edge; it is NOT a certified scan-phase window. */
    ESP_LOGW(TAG, "Full-frame edge synchronization only; optical phase model UNVERIFIED");
    uint64_t full_tx_max = run(panel, io, true, false, 12, (uint32_t)(3 * min), (uint32_t)(2 * min));
    run_v1_vs_v2_qspi_bench(panel, io, min, max, high, full_tx_max);
    run_dynamic_phase_te(panel, io, min, max, high, full_tx_max);
    run_te_matrix_and_cancel_profile(panel, io, min, max, high, full_tx_max);
    run_rotation_async(panel, io, false, 0, 0, 0);
    run_rotation_async(panel, io, true, 1, (uint32_t)(3 * min), (uint32_t)(2 * min));
    run_rotation_async(panel, io, true, 2, (uint32_t)(3 * min), (uint32_t)(2 * min));
    /* ROI GRAM consists of one band. Budget accounts for its measured
     * maximum plus dispatch margin; must fit measured pulse high duration.
     * Pulse timing is only a diagnostic proxy, not a proven optical safe window. */
    uint64_t band_max = run(panel, io, false, true, 32, 0, 0);
    uint32_t budget = (uint32_t)(band_max + 100);
    uint32_t window = (uint32_t)(high < min ? high : min);
    ESP_LOGI(TAG, "TE admission pulse_window=%"PRIu32" budget=%"PRIu32"; optical safety UNVERIFIED", window, budget);
    check(budget < window ? ESP_OK : ESP_ERR_NOT_SUPPORTED, "ROI does not fit measured TE pulse");
    for (unsigned cycle = 0;; cycle++) {
        run(panel, io, true, true, 120, window, budget);
        ESP_LOGI(TAG, "CYCLE_PASS %u", cycle);
    }
}
