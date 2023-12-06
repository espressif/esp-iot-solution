/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NS2016_H
#define _NS2016_H

#include "esp_log.h"
#include "touch_panel.h"

#ifdef __cplusplus
extern "C" {
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
esp_err_t ns2016_init(const touch_panel_config_t *xpt_conf);

/**
 * @brief Deinitial touch panel
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ns2016_deinit(void);

/**
 * @brief Check if there is a press
 *
 * @return
 *      - 0 Not press
 *      - 1 pressed
 */
int ns2016_is_pressed(void);

/**
 * @brief Set touch rotate direction
 *
 * @param dir rotate direction
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ns2016_set_direction(touch_panel_dir_t dir);

/**
 * @brief Get raw data
 *
 * @param x Value of X axis direction
 * @param y Value of Y axis direction
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ns2016_get_rawdata(uint16_t *x, uint16_t *y);

/**
 * @brief Start run touch panel calibration
 *
 * @param screen LCD driver for display prompts
 * @param recalibrate Is calibration mandatory
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ns2016_calibration_run(const scr_driver_t *screen, bool recalibrate);

/**
 * @brief Start a sample for screen
 *
 * @param info a pointer of touch_panel_points_t contained touch information.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ns2016_sample(touch_panel_points_t* info);

#ifdef __cplusplus
}
#endif

#endif
