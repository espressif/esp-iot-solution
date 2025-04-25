/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LVGL port compatibility
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backward compatibility with LVGL 8
 */
typedef lv_disp_t lv_display_t;
typedef enum {
    LV_DISPLAY_ROTATION_0 = LV_DISP_ROT_NONE,
    LV_DISPLAY_ROTATION_90 = LV_DISP_ROT_90,
    LV_DISPLAY_ROTATION_180 = LV_DISP_ROT_180,
    LV_DISPLAY_ROTATION_270 = LV_DISP_ROT_270
} lv_disp_rotation_t;

#ifdef __cplusplus
}
#endif
