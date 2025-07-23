/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lvgl_fps_stats.h"

#if CONFIG_EXAMPLE_LVGL_FPS_STATS_ENABLE

#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lvgl_fps";

typedef struct {
    uint64_t start_time_us;
    uint64_t last_frame_time_us;
    uint64_t last_log_time_us;

    uint64_t total_frames;
    float instant_fps;
    float average_fps;
    float min_fps;
    float max_fps;

    uint64_t frame_times[CONFIG_EXAMPLE_LVGL_FPS_STATS_WINDOW_SIZE];
    uint8_t frame_index;
    uint8_t valid_frames;

    bool initialized;
} fps_stats_t;

static fps_stats_t g_fps_stats = {0};

void lvgl_fps_stats_init(void)
{
    if (g_fps_stats.initialized) {
        return;
    }

    uint64_t now = esp_timer_get_time();
    g_fps_stats.start_time_us = now;
    g_fps_stats.last_frame_time_us = 0;
    g_fps_stats.last_log_time_us = now;
    g_fps_stats.total_frames = 0;
    g_fps_stats.instant_fps = 0.0f;
    g_fps_stats.average_fps = 0.0f;
    g_fps_stats.min_fps = 999.0f;
    g_fps_stats.max_fps = 0.0f;
    g_fps_stats.frame_index = 0;
    g_fps_stats.valid_frames = 0;
    g_fps_stats.initialized = true;

    ESP_LOGI(TAG, "FPS statistics initialized");
}

void lvgl_fps_stats_record_frame(void)
{
    if (!g_fps_stats.initialized) {
        return;
    }

    uint64_t current_time = esp_timer_get_time();

    if (g_fps_stats.last_frame_time_us > 0) {
        uint64_t frame_interval = current_time - g_fps_stats.last_frame_time_us;
        if (frame_interval > 0) {
            g_fps_stats.instant_fps = 1000000.0f / frame_interval;

#if CONFIG_EXAMPLE_LVGL_FPS_STATS_MODE_DETAILED
            if (g_fps_stats.instant_fps > g_fps_stats.max_fps) {
                g_fps_stats.max_fps = g_fps_stats.instant_fps;
            }
            if (g_fps_stats.instant_fps < g_fps_stats.min_fps) {
                g_fps_stats.min_fps = g_fps_stats.instant_fps;
            }
#endif
        }
    }

    g_fps_stats.frame_times[g_fps_stats.frame_index] = current_time;
    g_fps_stats.frame_index = (g_fps_stats.frame_index + 1) % CONFIG_EXAMPLE_LVGL_FPS_STATS_WINDOW_SIZE;

    if (g_fps_stats.valid_frames < CONFIG_EXAMPLE_LVGL_FPS_STATS_WINDOW_SIZE) {
        g_fps_stats.valid_frames++;
    }

    if (g_fps_stats.valid_frames >= 2) {
        uint8_t oldest_index = (g_fps_stats.frame_index + CONFIG_EXAMPLE_LVGL_FPS_STATS_WINDOW_SIZE - g_fps_stats.valid_frames) % CONFIG_EXAMPLE_LVGL_FPS_STATS_WINDOW_SIZE;
        uint64_t window_duration = current_time - g_fps_stats.frame_times[oldest_index];
        if (window_duration > 0) {
            g_fps_stats.average_fps = (g_fps_stats.valid_frames - 1) * 1000000.0f / window_duration;
        }
    }

    g_fps_stats.total_frames++;
    g_fps_stats.last_frame_time_us = current_time;

    if ((current_time - g_fps_stats.last_log_time_us) >= (CONFIG_EXAMPLE_LVGL_FPS_STATS_LOG_INTERVAL_MS * 1000)) {
        g_fps_stats.last_log_time_us = current_time;

#if CONFIG_EXAMPLE_LVGL_FPS_STATS_MODE_DETAILED
        ESP_LOGI(TAG, "FPS: %.1f (avg: %.1f, min: %.1f, max: %.1f), frames: %llu",
                 g_fps_stats.instant_fps, g_fps_stats.average_fps,
                 g_fps_stats.min_fps, g_fps_stats.max_fps, g_fps_stats.total_frames);
#else
        ESP_LOGI(TAG, "FPS: %.1f, avg: %.1f, frames: %llu",
                 g_fps_stats.instant_fps, g_fps_stats.average_fps, g_fps_stats.total_frames);
#endif
    }
}

void lvgl_fps_stats_get_info(lvgl_fps_info_t *info)
{
    if (!info || !g_fps_stats.initialized) {
        return;
    }

    info->instant_fps = g_fps_stats.instant_fps;
    info->average_fps = g_fps_stats.average_fps;
    info->min_fps = g_fps_stats.min_fps;
    info->max_fps = g_fps_stats.max_fps;
    info->total_frames = g_fps_stats.total_frames;
    info->runtime_ms = (esp_timer_get_time() - g_fps_stats.start_time_us) / 1000;
}

void lvgl_fps_stats_reset(void)
{
    g_fps_stats.initialized = false;
    lvgl_fps_stats_init();
}

float lvgl_fps_stats_get_instant(void)
{
    return g_fps_stats.initialized ? g_fps_stats.instant_fps : 0.0f;
}

#endif
