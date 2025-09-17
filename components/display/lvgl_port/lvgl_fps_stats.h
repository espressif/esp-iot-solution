/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_EXAMPLE_LVGL_FPS_STATS_ENABLE

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float instant_fps;
    float average_fps;
    float min_fps;
    float max_fps;
    uint64_t total_frames;
    uint32_t runtime_ms;
} lvgl_fps_info_t;

void lvgl_fps_stats_init(void);
void lvgl_fps_stats_record_frame(void);
void lvgl_fps_stats_get_info(lvgl_fps_info_t *info);
void lvgl_fps_stats_reset(void);
float lvgl_fps_stats_get_instant(void);

#endif

#if CONFIG_EXAMPLE_LVGL_FPS_STATS_ENABLE
#define LVGL_FPS_STATS_RECORD_FRAME() lvgl_fps_stats_record_frame()
#define LVGL_FPS_STATS_INIT()         lvgl_fps_stats_init()
#else
#define LVGL_FPS_STATS_RECORD_FRAME() do { } while(0)
#define LVGL_FPS_STATS_INIT()         do { } while(0)
#endif

#ifdef __cplusplus
}
#endif
