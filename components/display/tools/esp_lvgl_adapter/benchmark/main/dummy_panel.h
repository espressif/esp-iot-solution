/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dummy_panel.h
 * @brief Dummy LCD panel and panel_io implementation for headless benchmarking
 *
 * This provides virtual LCD panel and panel_io handles that satisfy the
 * esp_lvgl_adapter requirements without any actual hardware operations.
 */

#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a dummy panel_io handle for headless testing
 *
 * @param[out] ret_io Pointer to store the created panel_io handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM if allocation fails
 */
esp_err_t dummy_panel_io_create(esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Create a dummy panel handle for headless testing
 *
 * @param[in] io Panel IO handle (can be the dummy one or NULL)
 * @param[out] ret_panel Pointer to store the created panel handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM if allocation fails
 */
esp_err_t dummy_panel_create(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
