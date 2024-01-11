/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bldc_control.h"

/**
 * @brief bldc init
 *
 * @param direction
 * @return esp_err_t
 */
esp_err_t hal_bldc_init(dir_enum_t direction);

/**
 * @brief set bldc start or stop
 *
 * @param status
 * @return esp_err_t
 */
esp_err_t hal_bldc_start_stop(uint8_t status);

/**
 * @brief set bldc speed
 *
 * @param speed
 * @return esp_err_t
 */
esp_err_t hal_bldc_set_speed(uint16_t speed);
