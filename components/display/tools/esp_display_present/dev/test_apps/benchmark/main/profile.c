/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Benchmark-only wall-time/payload counters. Nested/parallel stages are not additive. */
#include <inttypes.h>
#include "esp_display_present.h"
#include "pixel/present_executor.h"
#include "core/present_engine.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
typedef struct {
    uint64_t n, us, max_us, bytes, errors;
} metric_t;
enum {BEGIN, ACQUIRE, SUBMIT, COMMIT, CPU, HARDWARE, FALLBACK, CACHE_OUT, CACHE_IN, PANEL, STAGES};
static const char *names[STAGES] = {"begin", "acquire", "submit", "commit", "cpu_exec", "hw_exec", "fallback", "cache_out", "cache_in", "panel_submit"};
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static metric_t metrics[STAGES];
static bool enabled;
static void IRAM_ATTR record(unsigned stage, uint64_t start, uint64_t bytes, esp_err_t error)
{
    uint64_t us = esp_timer_get_time() - start;
    bool isr = xPortInIsrContext();
    if (isr) {
        portENTER_CRITICAL_ISR(&lock);
    } else {
        portENTER_CRITICAL(&lock);
    }
    if (enabled) {
        metric_t *m = &metrics[stage]; m->n++; m->us += us; m->bytes += bytes; m->errors += error != ESP_OK;
        if (us > m->max_us) {
            m->max_us = us;
        }
    }
    if (isr) {
        portEXIT_CRITICAL_ISR(&lock);
    } else {
        portEXIT_CRITICAL(&lock);
    }
}
void bench_profile_reset(void)
{
    portENTER_CRITICAL(&lock);
    for (unsigned i = 0; i < STAGES; i++)metrics[i] = (metric_t) {
        0
    };
    enabled = true;
    portEXIT_CRITICAL(&lock);
}
void bench_profile_report(const char *label)
{
    metric_t result[STAGES];
    portENTER_CRITICAL(&lock);
    enabled = false;
    for (unsigned i = 0; i < STAGES; i++) {
        result[i] = metrics[i];
    }
    portEXIT_CRITICAL(&lock);
    for (unsigned i = 0; i < STAGES; i++) {
        metric_t *m = &result[i];
        ESP_LOGI("PRESENT_PROFILE", "label=%s stage=%s n=%"PRIu64" sum_us=%"PRIu64" avg_us=%"PRIu64" max_us=%"PRIu64" bytes=%"PRIu64" errors=%"PRIu64, label, names[i], m->n, m->us, m->n ? m->us / m->n : 0, m->max_us, m->bytes, m->errors);
    }
}
esp_err_t __real_esp_display_present_engine_begin_next_frame(esp_display_present_engine_t*, const esp_display_present_frame_request_t*);
esp_err_t __wrap_esp_display_present_engine_begin_next_frame(esp_display_present_engine_t*p, const esp_display_present_frame_request_t*r)
{
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_display_present_engine_begin_next_frame(p, r); record(BEGIN, t, 0, e); return e;
}
esp_err_t __real_esp_display_present_engine_acquire_buffer(esp_display_present_engine_t*, esp_display_present_lease_t*);
esp_err_t __wrap_esp_display_present_engine_acquire_buffer(esp_display_present_engine_t*p, esp_display_present_lease_t*b)
{
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_display_present_engine_acquire_buffer(p, b); record(ACQUIRE, t, 0, e); return e;
}
esp_err_t __real_esp_display_present_engine_submit_buffer(esp_display_present_engine_t*, const esp_display_present_lease_t*, const esp_display_present_engine_area_t*, size_t);
esp_err_t __wrap_esp_display_present_engine_submit_buffer(esp_display_present_engine_t*p, const esp_display_present_lease_t*b, const esp_display_present_engine_area_t*a, size_t stride)
{
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_display_present_engine_submit_buffer(p, b, a, stride); record(SUBMIT, t, 0, e); return e;
}
esp_err_t __real_esp_display_present_engine_commit_frame(esp_display_present_engine_t*, const esp_display_present_frame_coverage_t*);
esp_err_t __wrap_esp_display_present_engine_commit_frame(esp_display_present_engine_t*p, const esp_display_present_frame_coverage_t*s)
{
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_display_present_engine_commit_frame(p, s); record(COMMIT, t, 0, e); return e;
}
esp_err_t __real_esp_display_present_executor_run(const esp_display_present_executor_dispatch_t*, const esp_display_present_copy_request_t*, esp_display_present_executor_selection_t*);
esp_err_t __wrap_esp_display_present_executor_run(const esp_display_present_executor_dispatch_t*d, const esp_display_present_copy_request_t*r, esp_display_present_executor_selection_t*out)
{
    esp_display_present_executor_selection_t local; esp_display_present_executor_selection_t*s = out ? out : &local;
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_display_present_executor_run(d, r, s);
    if (s->executor) {
        esp_display_present_executor_caps_t caps;
        if (esp_display_present_executor_get_caps(s->executor, &caps) == ESP_OK) {
            bool cpu = caps.name && caps.name[0] == 'c' && caps.name[1] == 'p' && caps.name[2] == 'u' && !caps.name[3];
            uint64_t bytes = e == ESP_OK ? (uint64_t)r->src_area.width * r->src_area.height * (r->src.layout.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3) : 0;
            record(cpu ? CPU : HARDWARE, t, bytes, e);
            if (s->used_fallback) {
                record(FALLBACK, t, bytes, e);
            }
        }
    }
    return e;
}
#include "esp_cache.h"
esp_err_t __real_esp_cache_msync(void*, size_t, int);
esp_err_t IRAM_ATTR __wrap_esp_cache_msync(void*p, size_t bytes, int flags)
{
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_cache_msync(p, bytes, flags);
    if (flags & ESP_CACHE_MSYNC_FLAG_DIR_C2M) {
        record(CACHE_OUT, t, bytes, e == ESP_ERR_NOT_SUPPORTED ? ESP_OK : e);
    }
    if (flags & ESP_CACHE_MSYNC_FLAG_DIR_M2C) {
        record(CACHE_IN, t, bytes, e == ESP_ERR_NOT_SUPPORTED ? ESP_OK : e);
    }
    return e;
}
#include "esp_lcd_panel_ops.h"
esp_err_t __real_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t, int, int, int, int, const void*);
esp_err_t __wrap_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p, int x, int y, int ex, int ey, const void*pixels)
{
    uint64_t t = esp_timer_get_time(); esp_err_t e = __real_esp_lcd_panel_draw_bitmap(p, x, y, ex, ey, pixels); record(PANEL, t, (uint64_t)(ex - x) * (ey - y) * 2, e); return e;
}
