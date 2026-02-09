/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if CONFIG_CONSOLE_CONTROL
#include "dm_motor.h"

/**
 * @brief Control panthera using console commands
 *
 * @param motor_control Motor control instance
 * @param app_manager App manager instance
 * @return esp_err_t ESP_OK if successful, otherwise ESP_ERR_INVALID_ARG
 */
esp_err_t app_console_init(damiao::Motor_Control* motor_control, Manager* app_manager);

#endif
