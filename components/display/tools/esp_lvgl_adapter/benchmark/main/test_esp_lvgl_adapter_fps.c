/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_esp_lv_adapter_fps.c
 * @brief ESP LVGL Adapter FPS Benchmark - Tests rendering performance across
 *        multiple resolutions in headless mode
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "esp_lv_adapter.h"
#include "esp_lv_adapter_display.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "dummy_panel.h"

static const char *TAG = "adapter_fps_test";

#define FPS_STARTUP_THRESHOLD        (30)
#define FPS_STABLE_COUNT_REQUIRED    (3)
#define FPS_MAX_SAMPLES              (200)
#define FPS_MONITOR_STARTUP_DELAY_MS (1000)
#define FPS_END_DETECTION_RATIO      (0.3)     /* 30% of recent minimum triggers end detection */
#define FPS_CONSECUTIVE_END_REQUIRED (10)
#define FPS_ABSOLUTE_MIN_THRESHOLD   (3)

#define FPS_TASK_EXIT_BIT   (1 << 0)
#define BENCHMARK_DONE_BIT  (1 << 1)

typedef enum {
    FPS_STATE_STARTUP,
    FPS_STATE_STABLE,
    FPS_STATE_ENDING
} fps_state_t;

typedef struct {
    SemaphoreHandle_t mutex;
    EventGroupHandle_t event_group;
    TaskHandle_t task;
    lv_display_t *disp;
    bool finished;
    fps_state_t state;
    uint32_t state_counter;
    uint32_t samples[FPS_MAX_SAMPLES];
    uint32_t sample_count;
    uint64_t fps_sum;
    uint32_t fps_min;
    uint32_t fps_max;
    uint32_t stable_fps_threshold;
    uint32_t consecutive_low_count;
    uint32_t valid_sample_start;
    uint32_t valid_sample_end;
    uint32_t stable_sample_count;
    uint64_t stable_fps_sum;
} fps_monitor_ctx_t;

typedef struct {
    uint16_t width;
    uint16_t height;
} test_resolution_t;

static fps_monitor_ctx_t s_fps_ctx;

/* Test resolutions: 128x64 to 2048x1080 */
static const test_resolution_t k_test_resolutions[] = {
    {128,  64}, {160,  80},
    {128,  128}, {128,  160}, {240,  135}, {240,  240}, {240,  320},
    {320,  240}, {320,  320}, {320,  480},
    {360,  360}, {368,  448}, {390,  390}, {400,  240}, {400,  400},
    {412,  412}, {454,  454}, {480,  272}, {480,  320}, {480,  480},
    {600,  600}, {640,  360}, {640,  480},
    {720,  480}, {720,  720}, {720,  1280}, {800,  256}, {800,  480},
    {800,  600}, {960,  540}, {1024, 576}, {1024, 600}, {1024, 768},
    {1080, 1920}, {1152, 864}, {1280, 400}, {1280, 480}, {1280, 720},
    {1280, 768}, {1280, 800}, {1280, 1024}, {1360, 768}, {1366, 768},
    {1400, 1050}, {1440, 900}, {1600, 900}, {1680, 1050},
    {1920, 360}, {1920, 540}, {1920, 720}, {1920, 1080}, {1920, 1200},
    {2048, 1080},
};

#define TEST_RESOLUTION_COUNT  (sizeof(k_test_resolutions) / sizeof(k_test_resolutions[0]))

static void fps_monitor_task(void *arg);
static void benchmark_end_cb(const lv_demo_benchmark_summary_t *summary);

static void fps_ctx_reset(fps_monitor_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    ctx->mutex = NULL;
    ctx->event_group = NULL;
    ctx->task = NULL;
    ctx->disp = NULL;
    ctx->finished = false;
    ctx->state = FPS_STATE_STARTUP;
    ctx->state_counter = 0;
    ctx->sample_count = 0;
    ctx->fps_sum = 0;
    ctx->fps_min = UINT32_MAX;
    ctx->fps_max = 0;
    ctx->stable_fps_threshold = FPS_STARTUP_THRESHOLD;
    ctx->consecutive_low_count = 0;
    ctx->valid_sample_start = 0;
    ctx->valid_sample_end = 0;
    ctx->stable_sample_count = 0;
    ctx->stable_fps_sum = 0;
}

static esp_err_t fps_ctx_start(fps_monitor_ctx_t *ctx, lv_display_t *disp)
{
    if (!ctx || !disp) {
        return ESP_ERR_INVALID_ARG;
    }

    fps_ctx_reset(ctx);
    ctx->disp = disp;

    /* Create event group for synchronization */
    ctx->event_group = xEventGroupCreate();
    if (!ctx->event_group) {
        fps_ctx_reset(ctx);
        return ESP_ERR_NO_MEM;
    }

    /* Create mutex for statistics protection */
    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex) {
        vEventGroupDelete(ctx->event_group);
        fps_ctx_reset(ctx);
        return ESP_ERR_NO_MEM;
    }

    /* Enable FPS statistics collection */
    esp_err_t ret = esp_lv_adapter_fps_stats_enable(disp, true);
    if (ret != ESP_OK) {
        vSemaphoreDelete(ctx->mutex);
        vEventGroupDelete(ctx->event_group);
        fps_ctx_reset(ctx);
        ESP_LOGE(TAG, "Failed to enable FPS statistics: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create monitoring task */
    BaseType_t task_ret = xTaskCreate(fps_monitor_task, "fps_mon", 4096, ctx, 4, &ctx->task);
    if (task_ret != pdPASS) {
        esp_lv_adapter_fps_stats_enable(disp, false);
        vSemaphoreDelete(ctx->mutex);
        vEventGroupDelete(ctx->event_group);
        fps_ctx_reset(ctx);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void fps_ctx_stop(fps_monitor_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    /* Wait for task to exit */
    if (ctx->event_group && ctx->task) {
        EventBits_t bits = xEventGroupWaitBits(ctx->event_group,
                                               FPS_TASK_EXIT_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(3000));
        if ((bits & FPS_TASK_EXIT_BIT) == 0) {
            ESP_LOGW(TAG, "FPS monitor task exit timed out");
        }
    }

    /* Disable FPS statistics */
    if (ctx->disp) {
        esp_err_t ret = esp_lv_adapter_fps_stats_enable(ctx->disp, false);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to disable FPS statistics: %s", esp_err_to_name(ret));
        }
    }

    /* Clean up resources */
    if (ctx->mutex) {
        vSemaphoreDelete(ctx->mutex);
        ctx->mutex = NULL;
    }

    if (ctx->event_group) {
        vEventGroupDelete(ctx->event_group);
        ctx->event_group = NULL;
    }

    ctx->task = NULL;
    ctx->disp = NULL;
    ctx->finished = false;
    ctx->state = FPS_STATE_STARTUP;
    ctx->state_counter = 0;
    ctx->sample_count = 0;
    ctx->fps_sum = 0;
    ctx->fps_min = UINT32_MAX;
    ctx->fps_max = 0;
}

static void fps_monitor_task(void *arg)
{
    fps_monitor_ctx_t *ctx = (fps_monitor_ctx_t *)arg;
    if (!ctx || !ctx->disp) {
        ESP_LOGE(TAG, "Invalid FPS monitor task parameters");
        vTaskDelete(NULL);
        return;
    }

    lv_display_t *disp = ctx->disp;
    uint32_t startup_samples[5] = {0};
    uint32_t startup_sample_count = 0;

    while (!ctx->finished) {
        uint32_t fps = 0;
        if (esp_lv_adapter_get_fps(disp, &fps) == ESP_OK && ctx->mutex) {
            /* Real-time FPS logging */
            ESP_LOGI(TAG, "[FPS] Current: %lu fps", (unsigned long)fps);

            if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
                switch (ctx->state) {
                case FPS_STATE_STARTUP:
                    if (fps > 0) {
                        if (startup_sample_count == 0) {
                            ESP_LOGI(TAG, "[STARTUP] Calibrating FPS threshold...");
                        }

                        if (startup_sample_count < 5) {
                            startup_samples[startup_sample_count++] = fps;
                        }

                        if (startup_sample_count == 5) {
                            uint32_t sum = 0;
                            for (int i = 0; i < 5; i++) {
                                sum += startup_samples[i];
                            }
                            ctx->stable_fps_threshold = (uint32_t)(sum / 5 * 0.8);
                            startup_sample_count++;
                        }

                        if (fps >= ctx->stable_fps_threshold) {
                            ctx->state_counter++;
                            if (ctx->state_counter >= FPS_STABLE_COUNT_REQUIRED) {
                                ctx->state = FPS_STATE_STABLE;
                                ctx->state_counter = 0;
                                ctx->valid_sample_start = ctx->sample_count;
                                ESP_LOGI(TAG, "[STABLE] Starting data collection (threshold: %lu fps)",
                                         (unsigned long)ctx->stable_fps_threshold);
                            }
                        } else {
                            ctx->state_counter = 0;
                        }
                    }
                    break;

                case FPS_STATE_STABLE:
                    if (ctx->sample_count < FPS_MAX_SAMPLES) {
                        ctx->samples[ctx->sample_count++] = fps;
                        ctx->stable_sample_count++;
                        ctx->stable_fps_sum += fps;

                        uint32_t end_threshold = FPS_ABSOLUTE_MIN_THRESHOLD;
                        if (ctx->stable_sample_count >= 20) {
                            uint32_t min_recent_fps = UINT32_MAX;
                            uint32_t start_idx = (ctx->sample_count > 20) ? (ctx->sample_count - 20) : ctx->valid_sample_start;

                            for (uint32_t i = start_idx; i < ctx->sample_count; i++) {
                                if (ctx->samples[i] < min_recent_fps && ctx->samples[i] > 0) {
                                    min_recent_fps = ctx->samples[i];
                                }
                            }

                            if (min_recent_fps != UINT32_MAX) {
                                uint32_t dynamic_threshold = (uint32_t)(min_recent_fps * FPS_END_DETECTION_RATIO);
                                end_threshold = (dynamic_threshold > FPS_ABSOLUTE_MIN_THRESHOLD) ?
                                                dynamic_threshold : FPS_ABSOLUTE_MIN_THRESHOLD;
                            }
                        }

                        if (fps <= end_threshold) {
                            ctx->consecutive_low_count++;

                            if (ctx->consecutive_low_count == FPS_CONSECUTIVE_END_REQUIRED - 1) {
                                ESP_LOGI(TAG, "[STABLE] Detecting potential end (current: %lu fps, threshold: %lu fps)",
                                         (unsigned long)fps, (unsigned long)end_threshold);
                            }

                            if (ctx->consecutive_low_count >= FPS_CONSECUTIVE_END_REQUIRED) {
                                ctx->valid_sample_end = ctx->sample_count - FPS_CONSECUTIVE_END_REQUIRED;
                                ctx->state = FPS_STATE_ENDING;
                                ctx->consecutive_low_count = 0;  /* Reset counter after entering ENDING state */
                                ESP_LOGI(TAG, "[ENDING] Benchmark completed. Valid samples: %lu",
                                         (unsigned long)(ctx->valid_sample_end - ctx->valid_sample_start));
                            }
                        } else {
                            ctx->consecutive_low_count = 0;
                        }

                        if (ctx->sample_count % 20 == 0) {
                            ESP_LOGI(TAG, "[STABLE] Collected %lu samples...", (unsigned long)ctx->sample_count);
                        }
                    }

                    ctx->fps_sum += fps;
                    if (fps < ctx->fps_min) {
                        ctx->fps_min = fps;
                    }
                    if (fps > ctx->fps_max) {
                        ctx->fps_max = fps;
                    }
                    break;

                case FPS_STATE_ENDING:
                    if (ctx->sample_count < FPS_MAX_SAMPLES) {
                        ctx->samples[ctx->sample_count++] = fps;
                    }
                    break;
                }
                xSemaphoreGive(ctx->mutex);
            }
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
    }

    if (ctx->mutex && xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (ctx->valid_sample_end == 0 && ctx->sample_count > ctx->valid_sample_start) {
            ctx->valid_sample_end = ctx->sample_count;
        }
        xSemaphoreGive(ctx->mutex);
    }

    if (ctx->event_group) {
        xEventGroupSetBits(ctx->event_group, FPS_TASK_EXIT_BIT);
    }
    ctx->task = NULL;
    vTaskDelete(NULL);
}

static int compare_uint32(const void *a, const void *b)
{
    return (*(uint32_t *)a - * (uint32_t *)b);
}

static uint32_t calculate_percentile(uint32_t *samples, uint32_t count, float percentile)
{
    if (count == 0) {
        return 0;
    }
    uint32_t index = (uint32_t)((count - 1) * percentile / 100.0f);
    return samples[index];
}

static void print_avg_fps(void)
{
    if (!s_fps_ctx.mutex) {
        ESP_LOGW(TAG, "Cannot print FPS: mutex is NULL");
        return;
    }

    if (xSemaphoreTake(s_fps_ctx.mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW(TAG, "Cannot print FPS: failed to acquire mutex");
        return;
    }

    uint32_t total_count = s_fps_ctx.sample_count;
    uint32_t valid_start = s_fps_ctx.valid_sample_start;
    uint32_t valid_end = s_fps_ctx.valid_sample_end;

    if (total_count == 0) {
        ESP_LOGW(TAG, "No samples collected");
        xSemaphoreGive(s_fps_ctx.mutex);
        return;
    }

    if (valid_end == 0 || valid_end <= valid_start) {
        valid_end = total_count;
    }

    uint32_t valid_count = valid_end - valid_start;
    if (valid_count == 0) {
        valid_start = 0;
        valid_end = total_count;
        valid_count = total_count;
    }

    uint32_t work_samples[FPS_MAX_SAMPLES];
    memcpy(work_samples, &s_fps_ctx.samples[valid_start], valid_count * sizeof(uint32_t));

    uint64_t sum = 0;
    uint32_t min_fps = UINT32_MAX;
    uint32_t max_fps = 0;
    for (uint32_t i = 0; i < valid_count; i++) {
        uint32_t sample = work_samples[i];
        sum += sample;
        if (sample < min_fps) {
            min_fps = sample;
        }
        if (sample > max_fps) {
            max_fps = sample;
        }
    }
    if (min_fps == UINT32_MAX) {
        min_fps = 0;
    }

    qsort(work_samples, valid_count, sizeof(uint32_t), compare_uint32);

    uint32_t avg_fps = (uint32_t)(sum / valid_count);
    uint32_t p10 = calculate_percentile(work_samples, valid_count, 10);
    uint32_t p25 = calculate_percentile(work_samples, valid_count, 25);
    uint32_t p50 = calculate_percentile(work_samples, valid_count, 50);
    uint32_t p75 = calculate_percentile(work_samples, valid_count, 75);
    uint32_t p90 = calculate_percentile(work_samples, valid_count, 90);

    uint32_t iqr = p75 - p25;
    float iqr_ratio = (p50 > 0) ? ((float)iqr / (float)p50 * 100.0f) : 0.0f;

    const char *stability_rating;
    if (iqr_ratio < 30.0f) {
        stability_rating = "Excellent";
    } else if (iqr_ratio < 60.0f) {
        stability_rating = "Good";
    } else if (iqr_ratio < 100.0f) {
        stability_rating = "Moderate";
    } else if (iqr_ratio < 150.0f) {
        stability_rating = "Variable";
    } else {
        stability_rating = "High Variance";
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Performance Report");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Samples: %lu (filtered: %lu)", (unsigned long)valid_count, (unsigned long)(total_count - valid_count));
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Key Metrics:");
    ESP_LOGI(TAG, "  TYPICAL:      %lu fps  (P50, median)", (unsigned long)p50);
    ESP_LOGI(TAG, "  GUARANTEED:   %lu fps  (P25, 75%% time)", (unsigned long)p25);
    ESP_LOGI(TAG, "  PEAK:         %lu fps  (P90, best 10%%)", (unsigned long)p90);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Statistics:");
    ESP_LOGI(TAG, "  Average:      %lu fps", (unsigned long)avg_fps);
    ESP_LOGI(TAG, "  Range:        %lu - %lu fps", (unsigned long)min_fps, (unsigned long)max_fps);
    ESP_LOGI(TAG, "  Stability:    %s (IQR: %.1f%%)", stability_rating, iqr_ratio);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Distribution: P10=%lu | P25=%lu | P50=%lu | P75=%lu | P90=%lu",
             (unsigned long)p10, (unsigned long)p25, (unsigned long)p50,
             (unsigned long)p75, (unsigned long)p90);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Recommendations:");
    if (p25 >= 40) {
        ESP_LOGI(TAG, "  [EXCELLENT] Smooth animations, high-refresh UI");
    } else if (p25 >= 25) {
        ESP_LOGI(TAG, "  [GOOD] Standard UI, basic animations");
    } else if (p25 >= 15) {
        ESP_LOGI(TAG, "  [ACCEPTABLE] Static/simple UI");
    } else {
        ESP_LOGI(TAG, "  [LIMITED] Consider optimization or lower resolution");
    }
    ESP_LOGI(TAG, "========================================");

    xSemaphoreGive(s_fps_ctx.mutex);
}

static void benchmark_end_cb(const lv_demo_benchmark_summary_t *summary)
{
    (void)summary;

    /* Log ending state if not already in ENDING state */
    if (s_fps_ctx.mutex && xSemaphoreTake(s_fps_ctx.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (s_fps_ctx.state != FPS_STATE_ENDING) {
            if (s_fps_ctx.sample_count > s_fps_ctx.valid_sample_start) {
                s_fps_ctx.valid_sample_end = s_fps_ctx.sample_count;
                ESP_LOGI(TAG, "[ENDING] Benchmark completed (callback triggered). Valid samples: %lu",
                         (unsigned long)(s_fps_ctx.valid_sample_end - s_fps_ctx.valid_sample_start));
            }
        }
        xSemaphoreGive(s_fps_ctx.mutex);
    }

    s_fps_ctx.finished = true;

    if (s_fps_ctx.task) {
        xTaskNotifyGive(s_fps_ctx.task);
    }

    print_avg_fps();

    if (s_fps_ctx.event_group) {
        xEventGroupSetBits(s_fps_ctx.event_group, BENCHMARK_DONE_BIT);
    }
}

static void run_headless_benchmark(uint16_t h_res, uint16_t v_res)
{
    memset(&s_fps_ctx, 0, sizeof(fps_monitor_ctx_t));

    ESP_LOGI(TAG, "Benchmarking %ux%u...", h_res, v_res);

    /* Create dummy panel_io and panel for headless operation */
    esp_lcd_panel_io_handle_t dummy_io = NULL;
    esp_lcd_panel_handle_t dummy_panel = NULL;

    TEST_ESP_OK(dummy_panel_io_create(&dummy_io));
    TEST_ESP_OK(dummy_panel_create(dummy_io, &dummy_panel));

    esp_lv_adapter_display_config_t headless_config = {
        .panel = dummy_panel,
        .panel_io = dummy_io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER,
            .rotation = ESP_LV_ADAPTER_ROTATE_0,
            .hor_res = h_res,
            .ver_res = v_res,
            .buffer_height = 50,
            .use_psram = false,
            .enable_ppa_accel = false,
            .require_double_buffer = false,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
    };

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(1000)));
    lv_display_t *headless_disp = esp_lv_adapter_register_display(&headless_config);
    TEST_ASSERT_NOT_NULL(headless_disp);

    TEST_ESP_OK(esp_lv_adapter_set_dummy_draw(headless_disp, true));
    lv_display_set_default(headless_disp);

    /* Print actual color format */
    lv_color_format_t cf = lv_display_get_color_format(headless_disp);
    const char *format_str = "Unknown";
    switch (cf) {
    case LV_COLOR_FORMAT_RGB565:
        format_str = "RGB565 (16-bit)";
        break;
    case LV_COLOR_FORMAT_RGB888:
        format_str = "RGB888 (24-bit)";
        break;
    case LV_COLOR_FORMAT_XRGB8888:
        format_str = "XRGB8888 (32-bit)";
        break;
    case LV_COLOR_FORMAT_ARGB8888:
        format_str = "ARGB8888 (32-bit with alpha)";
        break;
    default:
        format_str = "Other";
        break;
    }
    ESP_LOGI(TAG, "Display color format: %s", format_str);

    esp_lv_adapter_unlock();

    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(1000)));
    lv_demo_benchmark_set_end_cb(benchmark_end_cb);
    lv_demo_benchmark();
    esp_lv_adapter_unlock();

    vTaskDelay(pdMS_TO_TICKS(FPS_MONITOR_STARTUP_DELAY_MS));

    TEST_ESP_OK(fps_ctx_start(&s_fps_ctx, headless_disp));

    /* Wait for benchmark completion */
    xEventGroupWaitBits(
        s_fps_ctx.event_group,
        BENCHMARK_DONE_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY
    );

    /* Stop monitoring */
    fps_ctx_stop(&s_fps_ctx);

    /* Clean up display */
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(1000)));
    lv_obj_clean(lv_disp_get_scr_act(headless_disp));
    esp_lv_adapter_unlock();

    esp_err_t ret = esp_lv_adapter_unregister_display(headless_disp);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Display unregister failed: %s", esp_err_to_name(ret));
    }

    /* Clean up dummy panel and panel_io */
    if (dummy_panel) {
        esp_lcd_panel_del(dummy_panel);
    }
    if (dummy_io) {
        esp_lcd_panel_io_del(dummy_io);
    }
}

/* ============================================================================
 * Unity Test Hooks
 * ============================================================================ */

void setUp(void) {}
void tearDown(void) {}

/* ============================================================================
 * Test Cases
 * ============================================================================ */

/**
 * @brief FPS benchmark test across multiple resolutions (53 test cases)
 *
 * Measures rendering performance from 128x64 to 2048x1080 using headless displays.
 */
TEST_CASE("adapter fps benchmark", "[adapter][fps]")
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "FPS Benchmark Suite");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Testing %d resolutions", TEST_RESOLUTION_COUNT);

    for (size_t i = 0; i < TEST_RESOLUTION_COUNT; i++) {
        ESP_LOGI(TAG, "[%u/%u] Resolution: %ux%u",
                 i + 1, TEST_RESOLUTION_COUNT,
                 k_test_resolutions[i].width,
                 k_test_resolutions[i].height);

        run_headless_benchmark(k_test_resolutions[i].width, k_test_resolutions[i].height);
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Done");
    ESP_LOGI(TAG, "========================================");
}

/* ============================================================================
 * Application Entry Point
 * ============================================================================ */

/**
 * @brief Application main entry point
 *
 * Initializes the LVGL adapter in headless mode and starts the Unity test menu.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP LVGL Adapter FPS Benchmark Test");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "ESP-IDF Target: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());

    /* Initialize LVGL adapter */
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    TEST_ESP_OK(esp_lv_adapter_init(&adapter_config));
    TEST_ESP_OK(esp_lv_adapter_start());

    ESP_LOGI(TAG, "LVGL adapter initialized");

    /* Start Unity test runner */
    unity_run_menu();

    ESP_LOGI(TAG, "All tests completed");
}
