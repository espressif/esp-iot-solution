/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MATRIX_BUTTON_COMBINE(row_gpio, col_gpio) ((row_gpio)<<8 | (col_gpio))
#define MATRIX_BUTTON_SPLIT_COL(data) ((uint32_t)(data)&0xff)
#define MATRIX_BUTTON_SPLIT_ROW(data) (((uint32_t)(data) >> 8) & 0xff)

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
    int32_t row_gpio_num;        /**< GPIO number associated with the row */
    int32_t col_gpio_num;        /**< GPIO number associated with the column */
} button_matrix_config_t;

/**
 * @brief Initialize a button matrix keyboard.
 *
 * @param config Pointer to the button matrix key configuration.
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if the argument is NULL.
 *
 * @note When initializing the button matrix keyboard, the row GPIO pins will be set as outputs,
 *       and the column GPIO pins will be set as inputs, both with pull-down resistors enabled.
 */
esp_err_t button_matrix_init(const button_matrix_config_t *config);

/**
 * @brief Deinitialize a button in the matrix keyboard.
 *
 * @param row_gpio_num GPIO number of the row where the button is located.
 * @param col_gpio_num GPIO number of the column where the button is located.
 * @return
 *      - ESP_OK if the button is successfully deinitialized
 *
 * @note When deinitializing a button, please exercise caution and avoid deinitializing a button individually, as it may affect the proper functioning of other buttons in the same row or column.
 */
esp_err_t button_matrix_deinit(int row_gpio_num, int col_gpio_num);

/**
 * @brief Get the key level from the button matrix hardware.
 *
 * @param hardware_data Pointer to hardware-specific data containing information about row GPIO and column GPIO.
 * @return uint8_t[out] The key level read from the hardware.
 *
 * @note This function retrieves the key level from the button matrix hardware.
 *       The `hardware_data` parameter should contain information about the row and column GPIO pins,
 *       and you can access this information using the `MATRIX_BUTTON_SPLIT_COL` and `MATRIX_BUTTON_SPLIT_ROW` macros.
 */
uint8_t button_matrix_get_key_level(void *hardware_data);

#ifdef __cplusplus
}
#endif
