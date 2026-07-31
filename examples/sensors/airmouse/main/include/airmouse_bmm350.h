/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AIRMOUSE_BMM350_H
#define AIRMOUSE_BMM350_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "airmouse_bmi270.h"
#include "airmouse_config.h"
#include "bmm350.h"
#include "esp_err.h"
#include "i2c_bus.h"

typedef struct {
    float hard_iron[3];
    float soft_iron[3][3];
    bool calibrated;
} airmouse_bmm350_mag_calibration_t;

typedef struct {
    struct bmm350_dev bmm_dev;
    i2c_bus_handle_t i2c_bus;
    bool owns_i2c_bus;
    uint8_t i2c_addr;
    int64_t last_error_log_us;
    airmouse_bmm350_mag_calibration_t mag_cal;
    bool calibration_available;
} airmouse_bmm350_handle_t;

/**
 * @brief Initialize the BMM350 magnetometer.
 *
 * Reuses the BMI270 bus when MAG_I2C_SCL/SDA match IMU_I2C_SCL/SDA.
 * Otherwise creates a dedicated I2C bus for the magnetometer.
 */
airmouse_bmm350_handle_t *airmouse_bmm350_init(
    i2c_bus_handle_t imu_i2c_bus,
    const airmouse_hardware_config_t *hardware_config);

void airmouse_bmm350_deinit(airmouse_bmm350_handle_t *handle);

esp_err_t airmouse_bmm350_calibration_load(airmouse_bmm350_handle_t *handle);
esp_err_t airmouse_bmm350_calibration_save(airmouse_bmm350_handle_t *handle);
esp_err_t airmouse_bmm350_calibration_erase(void);
esp_err_t airmouse_bmm350_calibration_prepare_or_run(
    airmouse_bmm350_handle_t *handle,
    airmouse_bmi270_handle_t *bmi270_handle);
esp_err_t airmouse_bmm350_apply_calibration(
    airmouse_bmm350_handle_t *handle,
    const float raw_mag[3],
    float calibrated_mag[3]);
bool airmouse_bmm350_is_calibration_valid(
    const airmouse_bmm350_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif
