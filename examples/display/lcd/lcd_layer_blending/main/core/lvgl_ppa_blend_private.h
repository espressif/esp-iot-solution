/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*user_data_flow_cb_t)(void);

typedef void (*user_ppa_blend_notify_done_cb_t)(void);

/**
 * @brief Notify that the PPA blend operation is complete.
 *
 * This function notifies the LVGL task that the current PPA (Pixel Processing Accelerator) frame buffer
 * has been transmitted. It handles notifications from both ISR (Interrupt Service Routine) and non-ISR contexts.
 *
 * @param is_isr Indicates whether the function is called from an ISR (true) or from a normal task (false).
 *
 * @return true if a context switch is needed, false otherwise.
 */
bool lvgl_ppa_blend_notify_done(bool is_isr);

/**
 * @brief Sets the callback function to be called when the PPA blend operation is completed.
 *
 * This function registers a user-defined callback that will be executed once
 * the PPA (Post-Processing Accelerator) blend operation is finished. The callback
 * can be used to notify the application layer or trigger subsequent actions.
 *
 * @param cb A function pointer to the user-defined callback of type `user_ppa_blend_notify_done_cb_t`.
 *           The callback should not block or perform heavy computations, as it may
 *           be called in a time-critical context.
 */
void lvgl_ppa_blend_set_notify_done_cb(user_ppa_blend_notify_done_cb_t cb);

/**
 * @brief Callback function for LCD VSYNC (Vertical Sync) event.
 *
 * This function is triggered during the VSYNC event of the LCD panel. It handles
 * potential tearing avoidance and notifies the LVGL task from an ISR (Interrupt Service Routine).
 *
 * @param handle The handle to the LCD panel.
 *
 * @return true if a context switch is needed, false otherwise.
 *
 */
bool on_lcd_vsync_cb(esp_lcd_panel_handle_t handle, esp_lcd_dpi_panel_event_data_t *event_data, void *user_data);

#ifdef __cplusplus
}
#endif
