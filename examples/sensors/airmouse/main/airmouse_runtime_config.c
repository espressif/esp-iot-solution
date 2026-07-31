/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_runtime_config.h"

static bool airmouse_pose_config_equal(const airmouse_pose_config_t *a,
                                       const airmouse_pose_config_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    return a->forward_axis == b->forward_axis &&
           a->up_axis == b->up_axis &&
           a->screen_x_axis == b->screen_x_axis &&
           a->screen_y_axis == b->screen_y_axis &&
           a->sensitivity_centi == b->sensitivity_centi;
}

static bool airmouse_hardware_config_equal(
    const airmouse_hardware_config_t *a,
    const airmouse_hardware_config_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    return a->i2c_scl == b->i2c_scl &&
           a->i2c_sda == b->i2c_sda &&
           a->mag_i2c_scl == b->mag_i2c_scl &&
           a->mag_i2c_sda == b->mag_i2c_sda &&
           a->sdo_gpio_control == b->sdo_gpio_control &&
           a->sdo_pin == b->sdo_pin &&
           a->reset_button == b->reset_button;
}

static bool airmouse_knob_config_equal(const airmouse_knob_config_t *a,
                                       const airmouse_knob_config_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    return a->axis == b->axis &&
           a->cw_sign == b->cw_sign &&
           a->atan2_first_axis == b->atan2_first_axis &&
           a->atan2_second_axis == b->atan2_second_axis &&
           a->hold_angle_milli == b->hold_angle_milli;
}

static bool airmouse_space_switch_config_equal(
    const airmouse_space_switch_config_t *a,
    const airmouse_space_switch_config_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    return a->horizontal_axis == b->horizontal_axis &&
           a->left_sign == b->left_sign &&
           a->vertical_axis == b->vertical_axis &&
           a->up_sign == b->up_sign;
}

static bool airmouse_inference_config_equal(
    const airmouse_inference_config_t *a,
    const airmouse_inference_config_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    return a->enable == b->enable &&
           a->sample_interval_ms == b->sample_interval_ms;
}

uint32_t airmouse_runtime_config_get_change_flags(
    const airmouse_config_t *old_config,
    const airmouse_config_t *new_config)
{
    uint32_t flags = AIRMOUSE_CONFIG_CHANGE_NONE;

    if (old_config == NULL || new_config == NULL) {
        return flags;
    }

    if (!airmouse_pose_config_equal(&old_config->pose, &new_config->pose)) {
        flags |= AIRMOUSE_CONFIG_CHANGE_POSE;
    }
    if (!airmouse_knob_config_equal(&old_config->knob, &new_config->knob)) {
        flags |= AIRMOUSE_CONFIG_CHANGE_KNOB;
    }
    if (!airmouse_space_switch_config_equal(&old_config->space_switch,
                                            &new_config->space_switch)) {
        flags |= AIRMOUSE_CONFIG_CHANGE_SPACE_SWITCH;
    }
    if (!airmouse_inference_config_equal(&old_config->inference,
                                         &new_config->inference)) {
        flags |= AIRMOUSE_CONFIG_CHANGE_INFERENCE;
    }
    if (!airmouse_hardware_config_equal(&old_config->hardware,
                                        &new_config->hardware)) {
        flags |= AIRMOUSE_CONFIG_CHANGE_HARDWARE;
    }

    return flags;
}
