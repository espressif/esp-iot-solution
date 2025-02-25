/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <string>

/**
 * @brief Mapping from index to activity category names
 *
 */
extern std::array<std::string, 6> index_to_category;

/**
 * @brief Mean values for feature normalization
 *
 */
extern const float mean_array[561];

/**
 * @brief Standard deviation values for feature normalization
 *
 */
extern const float std_array[561];

/**
 * @brief Example test input data
 *
 */
extern const float test_inputs[3][561];
