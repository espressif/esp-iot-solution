/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button matrix key configuration.
 *        Just need to configure the GPIO associated with this GPIO in the matrix keyboard.
 *
 *        Matrix Keyboard Layout (3x3):
 *        ----------------------------------------
 *        |  Button 1  |  Button 2  |  Button 3  |
 *        |  (R1-C1)   |  (R1-C2)   |  (R1-C3)   |
 *        |--------------------------------------|
 *        |  Button 4  |  Button 5  |  Button 6  |
 *        |  (R2-C1)   |  (R2-C2)   |  (R2-C3)   |
 *        |--------------------------------------|
 *        |  Button 7  |  Button 8  |  Button 9  |
 *        |  (R3-C1)   |  (R3-C2)   |  (R3-C3)   |
 *        ----------------------------------------
 *
 *        - Button matrix key is driven using row scanning.
 *        - Buttons within the same column cannot be detected simultaneously,
 *          but buttons within the same row can be detected without conflicts.
 */
typedef struct {
    int32_t *row_gpios;        /**< GPIO number list for the row */
    int32_t *col_gpios;        /**< GPIO number list for the column */
    uint32_t row_gpio_num;     /**< Number of GPIOs associated with the row */
    uint32_t col_gpio_num;     /**< Number of GPIOs associated with the column */
} button_matrix_config_t;

/**
 * @brief Create a new button matrix device
 *
 * This function initializes and configures a button matrix device using the specified row and column GPIOs.
 * Each button in the matrix is represented as an independent button object, and its handle is returned in the `ret_button` array.
 *
 * @param[in] button_config Configuration for the button device, including callbacks and debounce parameters.
 * @param[in] matrix_config Configuration for the matrix, including row and column GPIOs and their counts.
 * @param[out] ret_button Array of handles for the buttons in the matrix.
 * @param[inout] size Pointer to the total number of buttons in the matrix. Must match the product of row and column GPIO counts.
 *                    On success, this value is updated to reflect the size of the button matrix.
 *
 * @return
 *     - ESP_OK: Successfully created the button matrix device.
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided, such as null pointers or mismatched matrix dimensions.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - ESP_FAIL: General failure, such as button creation failure for one or more buttons.
 *
 * @note
 * - Each row GPIO is configured as an output, while each column GPIO is configured as an input.
 * - The total number of buttons in the matrix must equal the product of the row and column GPIO counts.
 * - The `ret_button` array must be large enough to store handles for all buttons in the matrix.
 * - If any button creation fails, the function will free all allocated resources and return an error.
 */
esp_err_t iot_button_new_matrix_device(const button_config_t *button_config, const button_matrix_config_t *matrix_config, button_handle_t *ret_button, size_t *size);

#ifdef __cplusplus
}
#endif
