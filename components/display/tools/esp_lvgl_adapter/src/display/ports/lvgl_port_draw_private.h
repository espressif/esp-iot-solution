/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL 9.6 private software-draw interfaces used by the ESP PPA bridge.
 */

#pragma once

#include "lvgl_port_private.h"

/* LVGL 9.6 moved these public draw headers below include/lvgl. Older LVGL 9
 * releases expose the same interfaces below the component root. */
#if defined(__has_include)
#if __has_include("lvgl/draw/lv_draw.h")
#include "lvgl/draw/lv_draw.h"
#include "lvgl/draw/lv_draw_buf.h"
#include "lvgl/stdlib/lv_mem.h"
#include "lvgl/draw/lv_color.h"
#else
#include "src/draw/lv_draw.h"
#include "src/draw/lv_draw_buf.h"
#include "stdlib/lv_mem.h"
#include "misc/lv_color.h"
#endif
#else
#include "src/draw/lv_draw.h"
#include "src/draw/lv_draw_buf.h"
#include "stdlib/lv_mem.h"
#include "misc/lv_color.h"
#endif

#include "src/draw/sw/blend/lv_draw_sw_blend.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb888.h"
