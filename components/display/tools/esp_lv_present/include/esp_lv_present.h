/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file esp_lv_present.h
 * @brief Bind an LVGL 9 display to an existing esp_display_present presenter.
 *
 * PARTITION borrows the presenter's tile draw buffer. DIRECT and FULL render
 * into the presenter's rotating framebuffer lease. Only the task that calls
 * esp_lv_present_start() may pump LVGL (lv_timer_handler).
 */

#include <stdint.h>

#include "esp_err.h"
#include "esp_display_present.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an LVGL display on @p presenter and install the flush path.
 *
 * Rebinds the presenter producer to the calling task. Initializes LVGL and an
 * esp_timer tick on the first successful call. Supports PARTITION, DIRECT,
 * and FULL contracts.
 *
 * @param[in]  presenter Presenter already created by the application.
 * @param[out] ret_disp  Created LVGL display handle.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p presenter or @p ret_disp is NULL
 *      - ESP_ERR_INVALID_STATE: Binding already started
 *      - ESP_ERR_NOT_SUPPORTED: Unsupported contract or pixel format
 *      - ESP_ERR_NO_MEM: Failed to create the LVGL display
 */
esp_err_t esp_lv_present_start(esp_display_presenter_t *presenter,
                               lv_display_t **ret_disp);

/**
 * @brief Tear down the LVGL display bound by esp_lv_present_start().
 *
 * Cancels any in-flight presenter frame first. The presenter itself is not
 * deleted; borrowed draw buffers remain owned by the presenter.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Binding is not started
 */
esp_err_t esp_lv_present_stop(void);

/**
 * @brief Return frames committed to the presenter since the last start.
 *
 * @return Committed frame count, or 0 when the binding is not started.
 */
uint32_t esp_lv_present_get_frame_count(void);

#ifdef __cplusplus
}
#endif
