/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "lcd_ppa_blend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Refresh the LCD PPA blend
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed
 */
esp_err_t lcd_ppa_blend_refresh(void);

/**
 * @brief Set the buffer for the PPA blend operation.
 *
 * This function assigns the specified buffer to the output layer of the PPA blend operation
 * and updates the PPA blend configuration with the new output layer settings.
 *
 * @param buf Pointer to the buffer to be used for the output layer.
 */
void lcd_ppa_blend_set_buffer(void *buf);

/**
 * @brief Get the buffer for the PPA blend operation.
 *
 * This function retrieves the buffer assigned to the output layer of the PPA blend operation.
 *
 * @return void* Pointer to the buffer used for the output layer.
 */
void* lcd_ppa_blend_get_buffer(void);

/**
 * @brief Callback function for LCD VSYNC (Vertical Sync) event.
 *
 * This function is triggered during the VSYNC event of the LCD panel. It releases a semaphore
 * from an ISR (Interrupt Service Routine) and determines if a context switch is required.
 *
 * @return true if a context switch is needed, false otherwise.
 */
#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
bool lcd_ppa_on_lcd_vsync_cb(void);
#endif

#ifdef __cplusplus
}
#endif
