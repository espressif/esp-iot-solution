/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AIRMOUSE_BMI270_H
#define AIRMOUSE_BMI270_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "bmi270_api.h"
#include "airmouse_config.h"
#include "esp_err.h"
#include "i2c_bus.h"

typedef struct {
    bmi270_handle_t bmi_handle;
    i2c_bus_handle_t i2c_bus;
    float (*gyr_data)[3];
} airmouse_bmi270_handle_t;

airmouse_bmi270_handle_t *airmouse_bmi270_init(
    const airmouse_hardware_config_t *hardware_config);
void airmouse_bmi270_deinit(airmouse_bmi270_handle_t *handle);
esp_err_t airmouse_bmi270_read_imu_once(
    airmouse_bmi270_handle_t *handle,
    float accel_data[3],
    float gyr_data[3]);

#ifdef __cplusplus
}
#endif

#endif
