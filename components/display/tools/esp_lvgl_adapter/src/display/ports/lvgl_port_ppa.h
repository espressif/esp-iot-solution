/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL Port PPA - PPA hardware acceleration interface for LVGL
 */

#pragma once

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *   PUBLIC API
 **********************/

#if LVGL_VERSION_MAJOR < 9
/**
 * @brief Initialize PPA acceleration for LVGL v8 display driver
 *
 * @param drv Display driver to enable PPA acceleration for
 */
void lvgl_port_ppa_v8_init(lv_disp_drv_t *drv);
#else
/**
 * @brief Initialize PPA acceleration for LVGL v9 display
 *
 * @param display Display to enable PPA acceleration for
 */
void lvgl_port_ppa_v9_init(lv_display_t *display);
#endif

#ifdef __cplusplus
}
#endif
