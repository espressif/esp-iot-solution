/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdio>
#include <iostream>

class TouchChannel {
public:
    uint32_t max;
    uint32_t min;
    uint32_t data;
    double normalized_data;

    // TouchChannel() :
    //     max(0), min(UINT32_MAX), normalized_data(0), data(0) {}

    TouchChannel(uint32_t max = 0, uint32_t min = UINT32_MAX)
        : max(max), min(min),  data(0), normalized_data(0) {}

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

    void update_max_min(uint32_t input_data)
    {
        if (input_data > max) {
            max = input_data;
        }

        if (input_data < min) {
            min = input_data;
        }
    }

    void reset()
    {
        max = 0;
        min = UINT32_MAX;
        normalized_data = 0;
        data = 0;
    }

    void print()
    {
        std::cout << " max:" << max << " min:" << min << std::endl;
    }
};
