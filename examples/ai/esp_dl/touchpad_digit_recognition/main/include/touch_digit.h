/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "touch_channel.h"

#define ROW_CHANNEL_NUM 7    /*!< Number of row channels */
#define COL_CHANNEL_NUM 6    /*!< Number of column channels */
#define PRECISION 5          /*!< Precision for data processing */

/**
 * @brief Touch digit data structure
 */
typedef struct {
    TouchChannel row_data[ROW_CHANNEL_NUM];  /*!< Row channel data array */
    TouchChannel col_data[COL_CHANNEL_NUM];  /*!< Column channel data array */
} touch_digit_data_t;

/**
 * @brief Initialize touch digit recognition
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t touch_digit_init(void);

/**
 * @brief Print touch digit data
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t printf_touch_digit_data(void);

/**
 * @brief Start normalization process
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t touch_dight_begin_normalize(void);

/**
 * @brief End normalization process
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t touch_dight_end_normalize(void);

/**
 * @brief Get normalization state
 *
 * @return true if normalization is in progress
 * @return false if normalization is not in progress
 */
bool get_touch_dight_normalize_state(void);
