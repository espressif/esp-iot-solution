/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include <inttypes.h>
#include <stdatomic.h>
#include "bench_api.h"
#include "bench_cases.h"
#include "demos/lv_demos.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#ifdef BENCH_DRIVER_TRACE
#include "esp_lcd_present_trace.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "present_bench";
static atomic_bool finished;
static const bench_case_t *active;
static int fps, cpu, render, flush, scenes;
/* Disjoint byte-addressable heaps. Aggregate minima need not be simultaneous. */
static void memory_report(const char *phase, bool local_minimum)
{
    const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const uint32_t psram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    ESP_LOGI(TAG, "MEMORY mode=%s rotation=%u phase=%s internal_total=%u internal_free=%u psram_total=%u psram_free=%u internal_min_free=%u psram_min_free=%u",
             active ? active->name : "SUITE", active ? active->rotation : 0, phase,
             (unsigned)heap_caps_get_total_size(internal), (unsigned)heap_caps_get_free_size(internal),
             (unsigned)heap_caps_get_total_size(psram), (unsigned)heap_caps_get_free_size(psram),
             local_minimum ? (unsigned)heap_caps_get_minimum_free_size(internal) : 0,
             local_minimum ? (unsigned)heap_caps_get_minimum_free_size(psram) : 0);
}
static void end_cb(const lv_demo_benchmark_summary_t *s)
{
    scenes = s->valid_scene_cnt;
    fps = scenes ? s->total_avg_fps / scenes : 0;
    cpu = scenes ? s->total_avg_cpu / scenes : 0;
    render = scenes ? s->total_avg_render_time / scenes : 0;
    flush = scenes ? s->total_avg_flush_time / scenes : 0;
    for (lv_demo_benchmark_scene_dsc_t *p = s->scenes; p->create_cb; p++) {
        if (p->measurement_cnt) {
            ESP_LOGI(TAG, "SCENE mode=%s rotation=%u name=\"%s\" fps=%u cpu=%u%% render=%ums flush=%ums", active->name, active->rotation, p->name, (unsigned)(p->fps_avg / p->measurement_cnt), (unsigned)(p->cpu_avg_usage / p->measurement_cnt), (unsigned)(p->render_avg_time / p->measurement_cnt), (unsigned)(p->flush_avg_time / p->measurement_cnt));
        }
        p->cpu_avg_usage = p->fps_avg = p->render_avg_time = p->flush_avg_time = p->measurement_cnt = 0;
    }
    atomic_store_explicit(&finished, true, memory_order_release);
}
static void run(void *unused)
{
    (void)unused;
    memory_report("before_init", false);
    ESP_ERROR_CHECK(bench_init());
    vTaskDelay(pdMS_TO_TICKS(20));
    memory_report("after_init", false);
    ESP_LOGI(TAG, "SUITE_BEGIN board=%s implementation=%s cases=%u", BENCH_BOARD, BENCH_IMPLEMENTATION, (unsigned)(sizeof(bench_cases) / sizeof(bench_cases[0])));
    for (unsigned i = 0; i < sizeof(bench_cases) / sizeof(bench_cases[0]); i++) {
        active = &bench_cases[i];
#ifdef BENCH_ADAPTER
        if (!active->adapter_supported) {
            ESP_LOGI(TAG, "SKIP mode=%s rotation=%u reason=%s", active->name, active->rotation, active->skip_reason);
            continue;
        }
#endif
#ifdef BENCH_DRIVER_TRACE
        esp_lcd_present_trace_reset();
#endif
        memory_report("before_open", false);
        ESP_ERROR_CHECK(heap_caps_monitor_local_minimum_free_size_start());
        lv_display_t *disp = NULL;
        ESP_ERROR_CHECK(bench_open(active, &disp));
        memory_report("after_open", false);
        atomic_store(&finished, false);
        bench_meter_reset();
        bench_profile_reset();
        ESP_ERROR_CHECK(bench_lock());
        bench_buffer_audit_start(disp, active);
        lv_demo_benchmark_set_end_cb(end_cb);
        lv_demo_benchmark();
        lv_obj_invalidate(lv_display_get_screen_active(disp));
        bench_unlock();
        ESP_LOGI(TAG, "RUN mode=%s te_buffers=%u rotation=%u", active->name, active->compose, active->rotation);
#ifdef BENCH_DRIVER_TRACE
        esp_lcd_present_trace_mark_running(1);
#endif
        int64_t start = esp_timer_get_time();
        while (!atomic_load_explicit(&finished, memory_order_acquire) && esp_timer_get_time() - start < 150000000) {
            bench_pump();
        }
#ifdef BENCH_DRIVER_TRACE
        esp_lcd_present_trace_mark_running(0);
#endif
        bool complete = atomic_load_explicit(&finished, memory_order_acquire);
        int64_t elapsed = esp_timer_get_time() - start;
        uint32_t frames = bench_frame_count();
        ESP_ERROR_CHECK(bench_lock());
        bench_buffer_audit_stop(disp);
        bench_unlock();
        memory_report("after_run", true);
        ESP_ERROR_CHECK(heap_caps_monitor_local_minimum_free_size_stop());
        ESP_ERROR_CHECK(bench_close());
        vTaskDelay(pdMS_TO_TICKS(20));
        memory_report("after_close", false);
        bench_buffer_audit_report();
#ifdef BENCH_DRIVER_TRACE
        esp_lcd_present_trace_report(active->name, active->rotation);
#endif
        bench_meter_report(active->name);
        bench_profile_report(active->name);
        ESP_ERROR_CHECK(complete && scenes > 0 ? ESP_OK : ESP_ERR_TIMEOUT);
        ESP_LOGI(TAG, "PASS mode=%s te_buffers=%u rotation=%u frames=%u transport_fps=%.1f lvgl_fps=%d cpu=%d%% render=%dms flush=%dms scenes=%d", active->name, active->compose, active->rotation, (unsigned)frames, frames * 1000000.0 / elapsed, fps, cpu, render, flush, scenes);
    }
    ESP_LOGI(TAG, "SUITE_PASS board=%s implementation=%s", BENCH_BOARD, BENCH_IMPLEMENTATION);
    vTaskDelete(NULL);
}
void app_main(void)
{
    ESP_ERROR_CHECK(xTaskCreate(run, "benchmark", 24576, NULL, 6, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
