/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "imu_quaternion.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    imu_quat_axis_t forward_axis;
    imu_quat_axis_t up_axis;
    imu_quat_axis_t screen_x_axis;
    imu_quat_axis_t screen_y_axis;
    int sensitivity_centi;
} airmouse_pose_config_t;

typedef struct {
    int i2c_scl;
    int i2c_sda;
    int mag_i2c_scl;
    int mag_i2c_sda;
    bool sdo_gpio_control;
    int sdo_pin;
    int reset_button;
} airmouse_hardware_config_t;

typedef struct {
    int axis;
    int cw_sign;
    int atan2_first_axis;
    int atan2_second_axis;
    int hold_angle_milli;
} airmouse_knob_config_t;

typedef struct {
    int horizontal_axis;
    int left_sign;
    int vertical_axis;
    int up_sign;
} airmouse_space_switch_config_t;

typedef struct {
    bool enable;
    int sample_interval_ms;
} airmouse_inference_config_t;

typedef struct {
    airmouse_pose_config_t pose;
    airmouse_hardware_config_t hardware;
    airmouse_knob_config_t knob;
    airmouse_space_switch_config_t space_switch;
    airmouse_inference_config_t inference;
} airmouse_config_t;

void airmouse_config_set_default(airmouse_config_t *config);

esp_err_t airmouse_config_save(const char *json_str,
                               airmouse_config_t *config);
esp_err_t airmouse_config_export_json(const airmouse_config_t *config,
                                      char **json_str_out);

#ifdef __cplusplus
}
#endif
