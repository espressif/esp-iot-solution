/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>

#include "airmouse_bmi270.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t airmouse_gyro_bias_load(float gyro_bias_dps[3], bool *loaded);

esp_err_t airmouse_gyro_bias_save(const float gyro_bias_dps[3]);

esp_err_t airmouse_gyro_bias_erase(void);

esp_err_t airmouse_gyro_bias_prepare_or_run(
    airmouse_bmi270_handle_t *imu_handle,
    const airmouse_hardware_config_t *hardware_config,
    float gyro_bias_dps[3],
    bool *valid);

#ifdef __cplusplus
}
#endif
