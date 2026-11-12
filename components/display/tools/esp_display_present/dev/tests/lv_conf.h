/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Host-only LVGL configuration, not a product firmware configuration. */
#pragma once
#define LV_CONF_H
/* LVGL 9.6 uses format; earlier 9.x defaults to the same 16-bit depth. */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_USE_OS 0
#define LV_USE_STDLIB_MALLOC 1
#define LV_USE_STDLIB_STRING 1
#define LV_USE_STDLIB_SPRINTF 1
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_DRAW_SW 1
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0
