/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Runtime matrix for presentation modes supported by the initialized panel.
 * Each case owns a fresh presenter and LVGL display. */

#include <inttypes.h>

#include "esp_display_present.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "hw_init.h"

#include "demos/lv_demos.h"
#include "lvgl.h"
#include "esp_lv_present.h"

static const char *TAG = "lvgl_bench";

/* LVGL worker: prio 6, no core affinity, 1ms min delay. */
#define LVGL_BENCHMARK_TICK_MS 1
#define LVGL_BENCHMARK_TASK_PRIO 6
#define LVGL_BENCHMARK_TASK_STACK 8192

#define APP_BENCH_QUIESCE_TIMEOUT_MS 2000

typedef struct {
    esp_display_present_mode_t mode;
    uint8_t te_compose_buffers;
} benchmark_mode_t;

typedef struct {
    bool complete;
    int32_t avg_fps;
    int32_t avg_cpu;
    int32_t avg_render_time;
    int32_t avg_flush_time;
    int32_t valid_scene_count;
} benchmark_result_t;

static const benchmark_mode_t s_modes[] = {
    // {ESP_DISPLAY_PRESENT_MODE_NONE, 0},
    {ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, 0},
    {ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL, 0},
    // {ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, 0},
    {ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, 0},
    {ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, 0},
    {ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 1},
    {ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 2},
    {ESP_DISPLAY_PRESENT_MODE_AUTO, 0},
};

static const esp_display_present_rotation_t s_rotations[] = {
    ESP_DISPLAY_PRESENT_ROTATE_0,
    ESP_DISPLAY_PRESENT_ROTATE_90,
    ESP_DISPLAY_PRESENT_ROTATE_180,
    ESP_DISPLAY_PRESENT_ROTATE_270,
};

static benchmark_result_t s_result;
static benchmark_mode_t s_active_mode;
static esp_display_present_rotation_t s_active_rotation;

static const char *mode_name(esp_display_present_mode_t mode)
{
    static const char *const names[] = {
        [ESP_DISPLAY_PRESENT_MODE_NONE] = "NONE",
        [ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL] = "DOUBLE_FULL",
        [ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL] = "TRIPLE_FULL",
        [ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT] = "DOUBLE_DIRECT",
        [ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL] = "TRIPLE_PARTIAL",
        [ESP_DISPLAY_PRESENT_MODE_TE_SYNC] = "TE_SYNC",
        [ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL] = "DOUBLE_PARTIAL",
        [ESP_DISPLAY_PRESENT_MODE_AUTO] = "AUTO",
    };
    return mode <= ESP_DISPLAY_PRESENT_MODE_AUTO && names[mode] != NULL
           ? names[mode] : "UNKNOWN";
}

static esp_err_t get_case_frame_buffer_count(
    const esp_display_present_target_config_t *target,
    const benchmark_mode_t *test,
    esp_display_present_rotation_t rotation,
    uint8_t *out_count)
{
    esp_display_present_target_config_t config = *target;
    config.fb.mode = test->mode;
    config.hw.rotation = rotation;
    return esp_display_present_get_required_frame_buffer_count(&config, out_count);
}

static void benchmark_end_cb(const lv_demo_benchmark_summary_t *summary)
{
    s_result.valid_scene_count = summary->valid_scene_cnt;
    if (summary->valid_scene_cnt > 0) {
        s_result.avg_fps = summary->total_avg_fps / summary->valid_scene_cnt;
        s_result.avg_cpu = summary->total_avg_cpu / summary->valid_scene_cnt;
        s_result.avg_render_time =
            summary->total_avg_render_time / summary->valid_scene_cnt;
        s_result.avg_flush_time =
            summary->total_avg_flush_time / summary->valid_scene_cnt;
    }

    /* LVGL 9.5 resets scene_act but not these static accumulators when
     * lv_demo_benchmark() is called again. Copy the summary first, then make
     * the next matrix case independent from this one. */
    for (lv_demo_benchmark_scene_dsc_t *scene = summary->scenes;
            scene->create_cb != NULL; ++scene) {
        if (scene->measurement_cnt > 0) {
            ESP_LOGI(TAG,
                     "SCENE mode=%s te_buffers=%u rotation=%d name=\"%s\" "
                     "fps=%u cpu=%u%% render=%ums flush=%ums",
                     mode_name(s_active_mode.mode),
                     s_active_mode.te_compose_buffers,
                     (int)s_active_rotation, scene->name,
                     (unsigned)(scene->fps_avg / scene->measurement_cnt),
                     (unsigned)(scene->cpu_avg_usage /
                                scene->measurement_cnt),
                     (unsigned)(scene->render_avg_time /
                                scene->measurement_cnt),
                     (unsigned)(scene->flush_avg_time /
                                scene->measurement_cnt));
        }
        scene->cpu_avg_usage = 0;
        scene->fps_avg = 0;
        scene->render_avg_time = 0;
        scene->flush_avg_time = 0;
        scene->measurement_cnt = 0;
    }
    s_result.complete = true;
}

static void logical_size(esp_display_present_rotation_t rotation,
                         uint16_t *width, uint16_t *height)
{
    if (rotation == ESP_DISPLAY_PRESENT_ROTATE_90 ||
            rotation == ESP_DISPLAY_PRESENT_ROTATE_270) {
        *width = HW_LCD_V_RES;
        *height = HW_LCD_H_RES;
    } else {
        *width = HW_LCD_H_RES;
        *height = HW_LCD_V_RES;
    }
}

static void pump_lvgl_for(uint32_t duration_ms)
{
    const int64_t end_us = esp_timer_get_time() +
                           (int64_t)duration_ms * 1000;
    while (esp_timer_get_time() < end_us) {
        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms == LV_NO_TIMER_READY) {
            delay_ms = LVGL_BENCHMARK_TICK_MS;
        } else if (delay_ms > 500) {
            delay_ms = 500;
        }
        if (delay_ms == 0) {
            taskYIELD();
        } else {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

static bool pump_lvgl_until_complete(uint32_t timeout_ms)
{
    const int64_t end_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (!s_result.complete && esp_timer_get_time() < end_us) {
        pump_lvgl_for(LVGL_BENCHMARK_TICK_MS);
    }
    return s_result.complete;
}

static esp_err_t run_case(
    const esp_display_present_target_config_t *base,
    const benchmark_mode_t *test,
    esp_display_present_rotation_t rotation)
{
    esp_display_present_target_config_t display = *base;
    display.fb.mode = test->mode;
    display.drawbuf.lines = 50;
    display.drawbuf.buffers = 1;
    display.drawbuf.in_psram = false;
    display.drawbuf.te_compose_buffers = test->te_compose_buffers;
    display.hw.rotation = rotation;

    uint16_t width = 0;
    uint16_t height = 0;
    logical_size(rotation, &width, &height);
    const esp_display_presenter_config_t config = {
        .width = width,
        .height = height,
        .pixel_format = display.hw.input_pixel_format,
        .max_damage_areas = 32,
        .target = display,
    };
    esp_display_presenter_t *presenter = NULL;
    esp_err_t ret = esp_display_presenter_create(&config, &presenter);
    if (ret != ESP_OK) {
        return ret;
    }

    lv_display_t *disp = NULL;
    ret = esp_lv_present_start(presenter, &disp);
    if (ret != ESP_OK) {
        (void)esp_display_presenter_delete(presenter);
        return ret;
    }
    s_result = (benchmark_result_t) {
        0
    };
    s_active_mode = *test;
    s_active_rotation = rotation;
    lv_demo_benchmark_set_end_cb(benchmark_end_cb);
    lv_demo_benchmark();
    lv_obj_invalidate(lv_display_get_screen_active(disp));

    ESP_LOGI(TAG,
             "RUN mode=%s te_buffers=%u rotation=%d size=%ux%u timeout=%ds",
             mode_name(test->mode), test->te_compose_buffers, (int)rotation,
             width, height, CONFIG_APP_LVGL_BENCH_TIMEOUT_SECONDS);
    const int64_t start_us = esp_timer_get_time();
    const bool complete = pump_lvgl_until_complete(
                              CONFIG_APP_LVGL_BENCH_TIMEOUT_SECONDS * 1000U);
    const int64_t elapsed_us = esp_timer_get_time() - start_us;
    const uint32_t frames = esp_lv_present_get_frame_count();

    if (!complete) {
        ESP_LOGE(TAG, "benchmark timed out before LVGL summary");
    }

    ESP_ERROR_CHECK(esp_display_presenter_quiesce(
                        presenter, APP_BENCH_QUIESCE_TIMEOUT_MS));
    ESP_ERROR_CHECK(esp_lv_present_stop());
    ESP_ERROR_CHECK(esp_display_presenter_delete(presenter));

    if (!complete) {
        return ESP_ERR_TIMEOUT;
    }

    const double fps = elapsed_us > 0
                       ? (double)frames * 1000000.0 / (double)elapsed_us : 0.0;
    ESP_LOGI(TAG,
             "PASS mode=%s te_buffers=%u rotation=%d frames=%u transport_fps=%.1f "
             "lvgl_fps=%" PRId32 " cpu=%" PRId32 "%% render=%" PRId32
             "ms flush=%" PRId32 "ms scenes=%" PRId32,
             mode_name(test->mode), test->te_compose_buffers, (int)rotation,
             (unsigned)frames, fps, s_result.avg_fps, s_result.avg_cpu,
             s_result.avg_render_time, s_result.avg_flush_time,
             s_result.valid_scene_count);
    return ESP_OK;
}

static void lvgl_benchmark_task(void *arg)
{
    const esp_display_present_target_config_t *display = arg;
    unsigned round = 0;
    for (;;) {
        ++round;
        printf("\r\n####################################################################\r\n");
        ESP_LOGI(TAG, "matrix round=%u start", round);
        for (size_t mode_index = 0;
                mode_index < sizeof(s_modes) / sizeof(s_modes[0]);
                ++mode_index) {
            for (size_t rotation_index = 0;
                    rotation_index <
                    sizeof(s_rotations) / sizeof(s_rotations[0]);
                    ++rotation_index) {
                const benchmark_mode_t *test = &s_modes[mode_index];
                const esp_display_present_rotation_t rotation =
                    s_rotations[rotation_index];
                uint8_t frame_buffer_count = 0;
                esp_err_t ret = get_case_frame_buffer_count(
                                    display, test, rotation, &frame_buffer_count);
                if (ret == ESP_ERR_NOT_SUPPORTED) {
                    ESP_LOGI(TAG,
                             "SKIP mode=%s te_buffers=%u rotation=%d: "
                             "outside target capability matrix",
                             mode_name(test->mode), test->te_compose_buffers,
                             (int)rotation);
                    continue;
                }
                ESP_ERROR_CHECK(ret);
                ret = run_case(display, test, rotation);
                if (ret == ESP_ERR_NOT_SUPPORTED) {
                    ESP_LOGI(TAG,
                             "SKIP mode=%s te_buffers=%u rotation=%d: "
                             "driver rejected configuration",
                             mode_name(test->mode), test->te_compose_buffers,
                             (int)rotation);
                    continue;
                }
                ESP_ERROR_CHECK(ret);
            }
        }
        ESP_LOGI(TAG, "matrix round=%u complete", round);
#ifndef CONFIG_APP_LVGL_BENCH_LOOP
        ESP_LOGI(TAG, "single-run matrix complete; benchmark task stopped");
        vTaskDelete(NULL);
        return;
#endif
    }
}

void app_main(void)
{
    static esp_display_present_target_config_t display;
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI;
    const bool swap_bytes = false;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB;
    const bool swap_bytes = false;
#else
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
    const bool swap_bytes = true;
#endif

    const uint8_t bpp = hw_lcd_get_bits_per_pixel();
    const int te_gpio = hw_lcd_get_te_gpio();
    display = (esp_display_present_target_config_t) {
        .hw = {
            .panel_type = panel_type,
            .input_pixel_format = (bpp == 24)
            ? ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888
            : ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
            .swap_bytes = swap_bytes,
            .te_enabled = (te_gpio != GPIO_NUM_NC),
            .te_sync = {
                .gpio_num = te_gpio,
                .bus_freq_hz = hw_lcd_get_bus_freq_hz(),
                .data_lines = hw_lcd_get_bus_data_lines(),
            },
        },
        .fb = {
            .mode = ESP_DISPLAY_PRESENT_MODE_AUTO,
        },
    };

    uint8_t frame_buffer_count = 0;
    for (size_t mode = 0; mode < sizeof(s_modes) / sizeof(s_modes[0]); ++mode) {
        for (size_t rotation = 0;
                rotation < sizeof(s_rotations) / sizeof(s_rotations[0]); ++rotation) {
            uint8_t required = 0;
            esp_err_t ret = get_case_frame_buffer_count(
                                &display, &s_modes[mode], s_rotations[rotation], &required);
            if (ret == ESP_ERR_NOT_SUPPORTED) {
                continue;
            }
            ESP_ERROR_CHECK(ret);
            if (required > frame_buffer_count) {
                frame_buffer_count = required;
            }
        }
    }
    ESP_ERROR_CHECK(hw_lcd_init(&panel, &io, frame_buffer_count, HW_ROTATE_0));
    display.hw.panel = panel;
    display.hw.io = io;

#if HW_USE_TOUCH
    esp_lcd_touch_handle_t touch = NULL;
    (void)hw_touch_init(&touch, HW_ROTATE_0);
#endif

    BaseType_t ret = xTaskCreatePinnedToCore(
                         lvgl_benchmark_task, "lvgl_bench",
                         LVGL_BENCHMARK_TASK_STACK, &display,
                         LVGL_BENCHMARK_TASK_PRIO, NULL, tskNO_AFFINITY);
    ESP_ERROR_CHECK(ret == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
