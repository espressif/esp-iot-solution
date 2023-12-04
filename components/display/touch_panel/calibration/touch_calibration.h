/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_TOUCH_SCREEN_CALIBRATION_H_
#define _IOT_TOUCH_SCREEN_CALIBRATION_H_

#include "screen_driver.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Start run touch panel calibration
 *
 * @param screen LCD driver for display prompts
 * @param func_is_pressed pointer of function
 * @param func_read_rawdata pointer of function
 * @param recalibrate Is calibration mandatory
 * @return esp_err_t
 */
esp_err_t touch_calibration_run(const scr_driver_t *screen,
                                int (*func_is_pressed)(void),
                                esp_err_t (*func_read_rawdata)(uint16_t *x, uint16_t *y),
                                bool recalibrate);

/**
 * @brief transform raw data of touch panel to pixel coordinate value of screen
 *
 * @param x Value of X axis direction
 * @param y Value of Y axis direction
 * @return esp_err_t
 */
esp_err_t touch_calibration_transform(int32_t *x, int32_t *y);

#ifdef __cplusplus
}
#endif

#endif
