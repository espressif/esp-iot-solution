/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdio>
#include <cstring>
#include <iostream>
#include "touch_digit.h"

class TouchImage {
public:
    uint32_t row_length;
    uint32_t col_length;
    uint8_t *data;

    TouchImage() :
        row_length((ROW_CHANNEL_NUM - 1) * PRECISION), col_length((COL_CHANNEL_NUM - 1) * PRECISION), data(nullptr)
    {
        data = new uint8_t[row_length * col_length];
        this->clear();
    }

    ~TouchImage()
    {
        if (data != nullptr) {
            delete[] data;
            data = nullptr;
        }
    }

    void set_pixel(uint8_t x, uint8_t y, uint8_t value)
    {
        data[x + y * row_length] = value;
    }

    void clear()
    {
        memset(data, 0, sizeof(uint8_t) * row_length * col_length);
    }

    void print()
    {
        for (int y = 0; y < col_length; y++) { // 遍历行
            for (int x = 0; x < row_length; x++) { // 遍历列
                printf(" %c ", data[y * row_length + x] == 0 ? ' ' : '*');
            }
            printf("\n"); // 换行
        }
    }
};
