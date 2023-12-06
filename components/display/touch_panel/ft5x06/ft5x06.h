/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_FT5X06_H_
#define _IOT_FT5X06_H_

#include "driver/i2c.h"
#include "i2c_bus.h"
#include "touch_panel.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initial touch panel
 *
 * @param config Pointer to a structure with touch config arguments.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ft5x06_init(const touch_panel_config_t * config);

/**
 * @brief Deinitial touch panel
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ft5x06_deinit(void);

/**
 * @brief Check if there is a press
 *
 * @return
 *      - 0 Not press
 *      - 1 pressed
 */
int ft5x06_is_press(void);

/**
 * @brief Set touch rotate rotation
 *
 * @param dir rotate direction
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ft5x06_set_direction(touch_panel_dir_t dir);

/**
 * @brief Start a sample for screen
 *
 * @param info a pointer of touch_panel_points_t contained touch information.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ft5x06_sample(touch_panel_points_t* info);

#ifdef __cplusplus
}
#endif

#endif
