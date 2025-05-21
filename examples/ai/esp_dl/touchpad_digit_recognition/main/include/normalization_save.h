/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "touch_digit.h"

/**
 * @brief Set the normalization data object
 *
 * @param data Pointer to the touch_digit_data_t structure containing
 *             the normalization data to be saved.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t set_normalization_data(touch_digit_data_t *data);

/**
 * @brief Get the normalization data object
 *
 * @param data Pointer to the touch_digit_data_t structure where the
 *             retrieved normalization data will be stored.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t get_normalization_data(touch_digit_data_t *data);
