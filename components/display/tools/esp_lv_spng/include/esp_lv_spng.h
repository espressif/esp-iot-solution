/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of handle for the split PNG decoder
 */
typedef void *esp_lv_spng_decoder_handle_t;

/**
 * @brief Register the PNG decoder functions in LVGL
 *
 * @param ret_handle Pointer to the handle where the decoder handle will be stored
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* error codes on failure
 */
esp_err_t esp_lv_split_png_init(esp_lv_spng_decoder_handle_t *ret_handle);

/**
 * @brief Deinitialize the PNG decoder handle
 *
 * @param handle The handle to be deinitialized
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* error codes on failure
 */
esp_err_t esp_lv_split_png_deinit(esp_lv_spng_decoder_handle_t handle);

#ifdef __cplusplus
} /* extern "C" */
#endif
