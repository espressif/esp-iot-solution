/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
#include "soc/soc_caps.h"

#if CONFIG_SOC_PPA_SUPPORTED
#include "esp_memory_utils.h"

/* Conservative cutoffs from P4/S31 SRAM and PSRAM measurements. Copies and
 * opaque fills have cheap software paths, so reserve PPA for large PSRAM work. */
static inline bool lvgl_port_ppa_use_hw(uint32_t pixels, bool copy, const void *buffer)
{
    if (copy) {
        return pixels >= 128 * 1024 && esp_ptr_external_ram(buffer);
    }
#if CONFIG_IDF_TARGET_ESP32S31
    return pixels >= 128 * 128;
#else
    return pixels >= 64 * 64;
#endif
}
#endif

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
 * Display deletion restores unused blend handlers and releases shared clients
 * after their last display. Unowned offscreen tasks use software drawing.
 * Serialize display setup/deletion with LVGL drawing.
 *
 * @param display Display to enable PPA acceleration for
 */
void lvgl_port_ppa_v9_init(lv_display_t *display);
#endif

#ifdef __cplusplus
}
#endif
