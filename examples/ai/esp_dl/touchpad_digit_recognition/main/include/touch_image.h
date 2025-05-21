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

/**
 * @brief Touch image class for handling touch data visualization
 */
class TouchImage {
public:
    uint32_t row_length;    /*!< Length of the image row */
    uint32_t col_length;    /*!< Length of the image column */
    uint8_t *data;          /*!< Image data buffer */

    /**
     * @brief Construct a new Touch Image object
     */
    TouchImage() :
        row_length((ROW_CHANNEL_NUM - 1) * PRECISION), col_length((COL_CHANNEL_NUM - 1) * PRECISION), data(nullptr)
    {
        data = new uint8_t[row_length * col_length];
        this->clear();
    }

    /**
     * @brief Destroy the Touch Image object
     */
    ~TouchImage()
    {
        if (data != nullptr) {
            delete[] data;
            data = nullptr;
        }
    }

    /**
     * @brief Set pixel value at specified position
     *
     * @param x X coordinate
     * @param y Y coordinate
     * @param value Pixel value
     */
    void set_pixel(uint8_t x, uint8_t y, uint8_t value)
    {
        data[x + y * row_length] = value;
    }

    /**
     * @brief Clear all pixels to zero
     */
    void clear()
    {
        memset(data, 0, sizeof(uint8_t) * row_length * col_length);
    }

    /**
     * @brief Print image to console
     */
    void print()
    {
        for (int y = 0; y < col_length; y++) {
            for (int x = 0; x < row_length; x++) {
                printf(" %c ", data[y * row_length + x] == 0 ? ' ' : '*');
            }
            printf("\n");
        }
    }
};
