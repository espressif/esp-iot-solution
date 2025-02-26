/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "touch_digit.h"

esp_err_t set_normalization_data(touch_digit_data_t *data);

esp_err_t get_normalization_data(touch_digit_data_t *data);
