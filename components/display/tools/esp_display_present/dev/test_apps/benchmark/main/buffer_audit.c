/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Benchmark-only buffer verification and nonempty refresh counters. */
#include <inttypes.h>
#include <string.h>
#include "bench_api.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
static struct {
    const bench_case_t *test;
    uint64_t refresh, rendered, flushes, pixels;
    uint32_t bytes, min_bytes, max_bytes, internal, psram;
    uint32_t expected, logical_w, logical_h, render_mode;
    int64_t start, elapsed;
} meter;
static void events(lv_event_t *e)
{
    switch (lv_event_get_code(e)) {
    case LV_EVENT_REFR_READY: meter.refresh++; break;
    case LV_EVENT_RENDER_READY: meter.rendered++; break;
    case LV_EVENT_FLUSH_START: {
        lv_display_t *d = lv_event_get_target(e);
        lv_draw_buf_t *b = lv_display_get_buf_active(d);
        assert(b && b->data);
        if (meter.expected) {
            assert(b->data_size == meter.expected);
        }
        if (!meter.flushes) {
            meter.bytes = b->data_size;
            meter.min_bytes = meter.max_bytes = b->data_size;
            meter.internal = esp_ptr_internal(b->data);
            meter.psram = esp_ptr_external_ram(b->data);
        }
        if (b->data_size < meter.min_bytes) {
            meter.min_bytes = b->data_size;
        }
        if (b->data_size > meter.max_bytes) {
            meter.max_bytes = b->data_size;
        }
        const lv_area_t *a = lv_event_get_param(e);
        if (a) {
            meter.pixels += (uint64_t)lv_area_get_width(a) * lv_area_get_height(a);
        }
        meter.flushes++;
        break;
    }
    default: break;
    }
}
void bench_buffer_audit_start(lv_display_t *d, const bench_case_t *c)
{
    memset(&meter, 0, sizeof(meter)); meter.test = c;
    meter.logical_w = lv_display_get_horizontal_resolution(d);
    meter.logical_h = lv_display_get_vertical_resolution(d);
    meter.render_mode = lv_display_get_render_mode(d);
    bool partial = c->mode == B_DOUBLE_PARTIAL || c->mode == B_TRIPLE_PARTIAL;
    bool full = c->mode == B_DOUBLE_FULL || c->mode == B_TRIPLE_FULL;
    if (partial) {
        meter.expected = meter.logical_w * 50 * 2;
    }
#if defined(BENCH_ADAPTER) && defined(BENCH_BUFFER_LEGACY)
    if (partial) {
        meter.expected = HW_LCD_H_RES * 50 * 2;
    }
#endif
    if (full) {
        meter.expected = meter.logical_w * meter.logical_h * 2;
    }
    meter.start = esp_timer_get_time();
    lv_display_add_event_cb(d, events, LV_EVENT_ALL, NULL);
}
void bench_buffer_audit_stop(lv_display_t *d)
{
    meter.elapsed = esp_timer_get_time() - meter.start;
    lv_display_remove_event_cb_with_user_data(d, events, NULL);
}
void bench_buffer_audit_report(void)
{
    ESP_LOGI("BUFFER_AUDIT", "mode=%s rotation=%u logical_w=%u logical_h=%u render_mode=%u expected_bytes=%u active_bytes=%u min_bytes=%u max_bytes=%u internal=%u psram=%u input_tiles=%u compose=%u refresh=%"PRIu64" rendered=%"PRIu64" flushes=%"PRIu64" pixels=%"PRIu64" elapsed_us=%"PRIi64,
             meter.test->name, meter.test->rotation, meter.logical_w, meter.logical_h, meter.render_mode,
             meter.expected, meter.bytes, meter.min_bytes, meter.max_bytes, meter.internal, meter.psram,
             meter.test->tiles, meter.test->compose, meter.refresh, meter.rendered, meter.flushes, meter.pixels, meter.elapsed);
}
