/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Self-contained LVGL demo screen used during the LVGL-exclusive phase
 * of the handoff test: a full-screen background distinct from the GSP
 * scene, a title, an animated bar and a cycle counter label. */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create and activate the demo screen on @p display. */
void handoff_lvgl_screen_create(lv_display_t *display, int cycle, int cycles);

/** Delete the demo screen (children and animations go with it). */
void handoff_lvgl_screen_delete(void);

#ifdef __cplusplus
}
#endif
