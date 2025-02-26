/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "touch_channel.h"

#define ROW_CHANNEL_NUM 7
#define COL_CHANNEL_NUM 6
#define PRECISION 5

typedef struct {
    TouchChannel row_data[ROW_CHANNEL_NUM];    // 1,3,5,7,9,11,13
    TouchChannel col_data[COL_CHANNEL_NUM];    // 8,6,4,2,10,12
} touch_digit_data_t;

esp_err_t touch_digit_init(void);

esp_err_t printf_touch_digit_data(void);

esp_err_t touch_dight_begin_normalize(void);

esp_err_t touch_dight_end_normalize(void);

bool get_touch_dight_normalize_state(void);
