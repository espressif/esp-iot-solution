/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdio>
#include <iostream>

/**
 * @brief Touch channel class for managing touch channel data
 */
class TouchChannel {
public:
    uint32_t max;            /*!< Maximum value of the channel data */
    uint32_t min;            /*!< Minimum value of the channel data */
    uint32_t data;           /*!< Current raw data of the channel */
    double normalized_data;  /*!< Normalized data in range [0,1] */

    /**
     * @brief Construct a new Touch Channel object
     *
     * @param max Initial maximum value, defaults to 0
     * @param min Initial minimum value, defaults to UINT32_MAX
     */
    TouchChannel(uint32_t max = 0, uint32_t min = UINT32_MAX)
        : max(max), min(min),  data(0), normalized_data(0) {}

    /**
     * @brief Normalize the input data
     *
     * @param input_data Raw input data to be normalized
     */
    void normalize(uint32_t input_data)
    {
        data = input_data;
        normalized_data = double((data - min) * 1.0f / (max - min));

        if (normalized_data > 1.0) {
            normalized_data = 1.0;
        } else if (normalized_data < 0.0) {
            normalized_data = 0.0;
        }
    }

    /**
     * @brief Update the maximum and minimum values of the channel
     *
     * @param input_data Raw input data to update max/min values
     */
    void update_max_min(uint32_t input_data)
    {
        if (input_data > max) {
            max = input_data;
        }

        if (input_data < min) {
            min = input_data;
        }
    }

    /**
     * @brief Reset the channel data
     */
    void reset()
    {
        max = 0;
        min = UINT32_MAX;
        normalized_data = 0;
        data = 0;
    }

    /**
     * @brief Print the maximum and minimum values
     */
    void print()
    {
        std::cout << " max:" << max << " min:" << min << std::endl;
    }
};
