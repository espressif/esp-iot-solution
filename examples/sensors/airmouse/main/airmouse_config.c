/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_config.h"

#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "AIRMOUSE_CONFIG";

static int airmouse_default_pose_sensitivity_centi(void)
{
    return CONFIG_AIRMOUSE_POSE_GAIN_X_CENTI;
}

static esp_err_t axis6_from_string(const char *str, imu_quat_axis_t *axis)
{
    if (str == NULL || axis == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(str, "+X") == 0) {
        *axis = IMU_QUAT_AXIS_POS_X;
    } else if (strcmp(str, "-X") == 0) {
        *axis = IMU_QUAT_AXIS_NEG_X;
    } else if (strcmp(str, "+Y") == 0) {
        *axis = IMU_QUAT_AXIS_POS_Y;
    } else if (strcmp(str, "-Y") == 0) {
        *axis = IMU_QUAT_AXIS_NEG_Y;
    } else if (strcmp(str, "+Z") == 0) {
        *axis = IMU_QUAT_AXIS_POS_Z;
    } else if (strcmp(str, "-Z") == 0) {
        *axis = IMU_QUAT_AXIS_NEG_Z;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const char *axis6_to_string(imu_quat_axis_t axis)
{
    switch (axis) {
    case IMU_QUAT_AXIS_POS_X:
        return "+X";
    case IMU_QUAT_AXIS_NEG_X:
        return "-X";
    case IMU_QUAT_AXIS_POS_Y:
        return "+Y";
    case IMU_QUAT_AXIS_NEG_Y:
        return "-Y";
    case IMU_QUAT_AXIS_POS_Z:
        return "+Z";
    case IMU_QUAT_AXIS_NEG_Z:
        return "-Z";
    default:
        return "?";
    }
}

static esp_err_t axis3_from_string(const char *str, int *axis)
{
    if (str == NULL || axis == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(str, "X") == 0) {
        *axis = 0;
    } else if (strcmp(str, "Y") == 0) {
        *axis = 1;
    } else if (strcmp(str, "Z") == 0) {
        *axis = 2;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const char *axis3_to_string(int axis)
{
    switch (axis) {
    case 0:
        return "X";
    case 1:
        return "Y";
    case 2:
        return "Z";
    default:
        return "?";
    }
}

static int axis_dimension(int axis)
{
    switch (axis) {
    case IMU_QUAT_AXIS_POS_X:
    case IMU_QUAT_AXIS_NEG_X:
        return 0;
    case IMU_QUAT_AXIS_POS_Y:
    case IMU_QUAT_AXIS_NEG_Y:
        return 1;
    case IMU_QUAT_AXIS_POS_Z:
    case IMU_QUAT_AXIS_NEG_Z:
        return 2;
    default:
        return -1;
    }
}

static esp_err_t validate_pose_axes(int forward_axis,
                                    int up_axis,
                                    int screen_x_axis,
                                    int screen_y_axis)
{
    int f_dim = axis_dimension(forward_axis);
    int u_dim = axis_dimension(up_axis);
    int x_dim = axis_dimension(screen_x_axis);
    int y_dim = axis_dimension(screen_y_axis);

    if (f_dim < 0 || u_dim < 0 || x_dim < 0 || y_dim < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (f_dim == u_dim || f_dim == x_dim || f_dim == y_dim || x_dim == y_dim) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static bool gpio_value_is_valid(int gpio_num)
{
    return gpio_num >= 0 && gpio_num < GPIO_NUM_MAX;
}

static esp_err_t validate_hardware_pins(int i2c_scl,
                                        int i2c_sda,
                                        int mag_i2c_scl,
                                        int mag_i2c_sda,
                                        bool sdo_gpio_control,
                                        int sdo_pin,
                                        int reset_button)
{
    if (!gpio_value_is_valid(i2c_scl) ||
            !gpio_value_is_valid(i2c_sda) ||
            !gpio_value_is_valid(reset_button)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (i2c_scl == i2c_sda ||
            i2c_scl == reset_button ||
            i2c_sda == reset_button) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gpio_value_is_valid(mag_i2c_scl) ||
            !gpio_value_is_valid(mag_i2c_sda)) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool mag_bus_shared =
        (mag_i2c_scl == i2c_scl) && (mag_i2c_sda == i2c_sda);
    const bool mag_bus_partial_overlap =
        (mag_i2c_scl == i2c_scl) != (mag_i2c_sda == i2c_sda);

    if (mag_bus_partial_overlap) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mag_bus_shared) {
        if (mag_i2c_scl == mag_i2c_sda ||
                mag_i2c_scl == reset_button ||
                mag_i2c_sda == reset_button ||
                mag_i2c_scl == i2c_scl ||
                mag_i2c_scl == i2c_sda ||
                mag_i2c_sda == i2c_scl ||
                mag_i2c_sda == i2c_sda) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (!sdo_gpio_control) {
        return ESP_OK;
    }

    if (!gpio_value_is_valid(sdo_pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (i2c_scl == sdo_pin ||
            i2c_sda == sdo_pin ||
            mag_i2c_scl == sdo_pin ||
            mag_i2c_sda == sdo_pin ||
            sdo_pin == reset_button) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void resolve_default_knob_atan2_axes(int knob_axis,
                                            int *atan2_first_axis,
                                            int *atan2_second_axis)
{
    if (atan2_first_axis == NULL || atan2_second_axis == NULL) {
        return;
    }

    switch (knob_axis) {
    case 0:
        *atan2_first_axis = 1;
        *atan2_second_axis = 2;
        break;
    case 1:
        *atan2_first_axis = 2;
        *atan2_second_axis = 0;
        break;
    case 2:
    default:
        *atan2_first_axis = 0;
        *atan2_second_axis = 1;
        break;
    }
}

static esp_err_t validate_knob_config(int axis,
                                      int cw_sign,
                                      int atan2_first_axis,
                                      int atan2_second_axis,
                                      int hold_angle_milli)
{
    if (axis < 0 || axis > 2) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cw_sign != 1 && cw_sign != -1) {
        return ESP_ERR_INVALID_ARG;
    }

    if (atan2_first_axis < 0 || atan2_first_axis > 2 ||
            atan2_second_axis < 0 || atan2_second_axis > 2 ||
            atan2_first_axis == atan2_second_axis) {
        return ESP_ERR_INVALID_ARG;
    }

    if (hold_angle_milli <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t validate_space_switch_config(int horizontal_axis,
                                              int left_sign,
                                              int vertical_axis,
                                              int up_sign)
{
    if (horizontal_axis < 0 || horizontal_axis > 2 ||
            vertical_axis < 0 || vertical_axis > 2 ||
            horizontal_axis == vertical_axis) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((left_sign != 1 && left_sign != -1) ||
            (up_sign != 1 && up_sign != -1)) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t json_get_optional_int(cJSON *root,
                                       const char *key,
                                       int current_value,
                                       int *out_value)
{
    if (root == NULL || key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item == NULL) {
        *out_value = current_value;
        return ESP_OK;
    }

    if (!cJSON_IsNumber(item)) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_value = item->valueint;
    return ESP_OK;
}

static esp_err_t json_get_optional_bool(cJSON *root,
                                        const char *key,
                                        bool current_value,
                                        bool *out_value)
{
    if (root == NULL || key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item == NULL) {
        *out_value = current_value;
        return ESP_OK;
    }

    if (!cJSON_IsBool(item)) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_value = cJSON_IsTrue(item);
    return ESP_OK;
}

static esp_err_t json_get_optional_axis6(cJSON *root,
                                         const char *key,
                                         imu_quat_axis_t current_value,
                                         imu_quat_axis_t *out_value)
{
    if (root == NULL || key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item == NULL) {
        *out_value = current_value;
        return ESP_OK;
    }

    if (!cJSON_IsString(item)) {
        return ESP_ERR_INVALID_ARG;
    }

    return axis6_from_string(item->valuestring, out_value);
}

static esp_err_t json_get_optional_axis3(cJSON *root,
                                         const char *key,
                                         int current_value,
                                         int *out_value)
{
    if (root == NULL || key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item == NULL) {
        *out_value = current_value;
        return ESP_OK;
    }

    if (cJSON_IsString(item)) {
        return axis3_from_string(item->valuestring, out_value);
    }

    if (cJSON_IsNumber(item)) {
        *out_value = item->valueint;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static esp_err_t airmouse_config_parse_pose(cJSON *root,
                                            airmouse_pose_config_t *pose)
{
    imu_quat_axis_t forward_axis;
    imu_quat_axis_t up_axis;
    imu_quat_axis_t screen_x_axis;
    imu_quat_axis_t screen_y_axis;
    int sensitivity_centi;
    esp_err_t ret;

    if (root == NULL || pose == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    forward_axis = pose->forward_axis;
    up_axis = pose->up_axis;
    screen_x_axis = pose->screen_x_axis;
    screen_y_axis = pose->screen_y_axis;
    sensitivity_centi = pose->sensitivity_centi;

    if (axis_dimension(forward_axis) < 0) {
        forward_axis = IMU_QUAT_AXIS_NEG_X;
    }
    if (axis_dimension(up_axis) < 0) {
        up_axis = IMU_QUAT_AXIS_POS_Z;
    }
    if (axis_dimension(screen_x_axis) < 0) {
        screen_x_axis = IMU_QUAT_AXIS_POS_Y;
    }
    if (axis_dimension(screen_y_axis) < 0) {
        screen_y_axis = IMU_QUAT_AXIS_NEG_Z;
    }
    if (sensitivity_centi <= 0) {
        sensitivity_centi = airmouse_default_pose_sensitivity_centi();
    }

    ret = json_get_optional_axis6(root,
                                  "pose_forward_axis",
                                  forward_axis,
                                  &forward_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_axis6(root, "pose_up_axis", up_axis, &up_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_axis6(root,
                                  "pose_screen_x_axis",
                                  screen_x_axis,
                                  &screen_x_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_axis6(root,
                                  "pose_screen_y_axis",
                                  screen_y_axis,
                                  &screen_y_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "pose_sensitivity_centi",
                                sensitivity_centi,
                                &sensitivity_centi);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = validate_pose_axes(forward_axis,
                             up_axis,
                             screen_x_axis,
                             screen_y_axis);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "invalid pose axes: forward=%s, up=%s, screen_x=%s, screen_y=%s",
                 axis6_to_string(forward_axis),
                 axis6_to_string(up_axis),
                 axis6_to_string(screen_x_axis),
                 axis6_to_string(screen_y_axis));
        return ret;
    }

    if (sensitivity_centi <= 0) {
        ESP_LOGE(TAG, "invalid pose sensitivity: pose_sensitivity_centi=%d",
                 sensitivity_centi);
        return ESP_ERR_INVALID_ARG;
    }

    pose->forward_axis = forward_axis;
    pose->up_axis = up_axis;
    pose->screen_x_axis = screen_x_axis;
    pose->screen_y_axis = screen_y_axis;
    pose->sensitivity_centi = sensitivity_centi;

    ESP_LOGI(TAG, "pose_forward_axis = %s", axis6_to_string(pose->forward_axis));
    ESP_LOGI(TAG, "pose_up_axis = %s", axis6_to_string(pose->up_axis));
    ESP_LOGI(TAG, "pose_screen_x_axis = %s",
             axis6_to_string(pose->screen_x_axis));
    ESP_LOGI(TAG, "pose_screen_y_axis = %s",
             axis6_to_string(pose->screen_y_axis));
    ESP_LOGI(TAG, "pose_sensitivity_centi = %d", pose->sensitivity_centi);

    return ESP_OK;
}

static esp_err_t airmouse_config_parse_hardware(
    cJSON *root,
    airmouse_hardware_config_t *hardware)
{
    int i2c_scl;
    int i2c_sda;
    int mag_i2c_scl;
    int mag_i2c_sda;
    bool sdo_gpio_control;
    int sdo_pin;
    int reset_button;
    esp_err_t ret;

    if (root == NULL || hardware == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_scl = hardware->i2c_scl;
    i2c_sda = hardware->i2c_sda;
    mag_i2c_scl = hardware->mag_i2c_scl;
    mag_i2c_sda = hardware->mag_i2c_sda;
    sdo_gpio_control = hardware->sdo_gpio_control;
    sdo_pin = hardware->sdo_pin;
    reset_button = hardware->reset_button;

    ret = json_get_optional_int(root, "imu_i2c_scl", i2c_scl, &i2c_scl);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root, "imu_i2c_sda", i2c_sda, &i2c_sda);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "mag_i2c_scl",
                                mag_i2c_scl,
                                &mag_i2c_scl);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "mag_i2c_sda",
                                mag_i2c_sda,
                                &mag_i2c_sda);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_bool(root,
                                 "imu_sdo_gpio_control",
                                 sdo_gpio_control,
                                 &sdo_gpio_control);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root, "imu_sdo_pin", sdo_pin, &sdo_pin);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root, "reset_button", reset_button, &reset_button);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = validate_hardware_pins(i2c_scl,
                                 i2c_sda,
                                 mag_i2c_scl,
                                 mag_i2c_sda,
                                 sdo_gpio_control,
                                 sdo_pin,
                                 reset_button);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "invalid hardware pins: imu_i2c_scl=%d, imu_i2c_sda=%d, mag_i2c_scl=%d, mag_i2c_sda=%d, imu_sdo_gpio_control=%d, imu_sdo_pin=%d, reset_button=%d",
                 i2c_scl,
                 i2c_sda,
                 mag_i2c_scl,
                 mag_i2c_sda,
                 sdo_gpio_control,
                 sdo_pin,
                 reset_button);
        return ret;
    }

    hardware->i2c_scl = i2c_scl;
    hardware->i2c_sda = i2c_sda;
    hardware->mag_i2c_scl = mag_i2c_scl;
    hardware->mag_i2c_sda = mag_i2c_sda;
    hardware->sdo_gpio_control = sdo_gpio_control;
    hardware->sdo_pin = sdo_pin;
    hardware->reset_button = reset_button;

    ESP_LOGI(TAG, "imu_i2c_scl = %d", hardware->i2c_scl);
    ESP_LOGI(TAG, "imu_i2c_sda = %d", hardware->i2c_sda);
    ESP_LOGI(TAG, "mag_i2c_scl = %d", hardware->mag_i2c_scl);
    ESP_LOGI(TAG, "mag_i2c_sda = %d", hardware->mag_i2c_sda);
    ESP_LOGI(TAG, "imu_sdo_gpio_control = %d", hardware->sdo_gpio_control);
    if (hardware->sdo_gpio_control) {
        ESP_LOGI(TAG, "imu_sdo_pin = %d", hardware->sdo_pin);
    } else {
        ESP_LOGI(TAG, "imu_sdo_pin ignored because GPIO control is disabled");
    }
    ESP_LOGI(TAG, "reset_button = %d", hardware->reset_button);

    return ESP_OK;
}

static esp_err_t airmouse_config_parse_knob(cJSON *root,
                                            airmouse_knob_config_t *knob)
{
    int axis;
    int cw_sign;
    int atan2_first_axis;
    int atan2_second_axis;
    int hold_angle_milli;
    esp_err_t ret;

    if (root == NULL || knob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    axis = knob->axis;
    cw_sign = knob->cw_sign;
    atan2_first_axis = knob->atan2_first_axis;
    atan2_second_axis = knob->atan2_second_axis;
    hold_angle_milli = knob->hold_angle_milli;

    ret = json_get_optional_axis3(root, "knob_axis", axis, &axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root, "knob_cw_sign", cw_sign, &cw_sign);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_axis3(root,
                                  "knob_atan2_first_axis",
                                  atan2_first_axis,
                                  &atan2_first_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_axis3(root,
                                  "knob_atan2_second_axis",
                                  atan2_second_axis,
                                  &atan2_second_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "knob_hold_angle_milli",
                                hold_angle_milli,
                                &hold_angle_milli);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = validate_knob_config(axis,
                               cw_sign,
                               atan2_first_axis,
                               atan2_second_axis,
                               hold_angle_milli);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "invalid knob config: knob_axis=%d, knob_cw_sign=%d, knob_atan2_first_axis=%d, knob_atan2_second_axis=%d, knob_hold_angle_milli=%d",
                 axis,
                 cw_sign,
                 atan2_first_axis,
                 atan2_second_axis,
                 hold_angle_milli);
        return ret;
    }

    knob->axis = axis;
    knob->cw_sign = cw_sign;
    knob->atan2_first_axis = atan2_first_axis;
    knob->atan2_second_axis = atan2_second_axis;
    knob->hold_angle_milli = hold_angle_milli;

    ESP_LOGI(TAG, "knob_axis = %s", axis3_to_string(knob->axis));
    ESP_LOGI(TAG, "knob_cw_sign = %d", knob->cw_sign);
    ESP_LOGI(TAG, "knob_atan2_first_axis = %s",
             axis3_to_string(knob->atan2_first_axis));
    ESP_LOGI(TAG, "knob_atan2_second_axis = %s",
             axis3_to_string(knob->atan2_second_axis));
    ESP_LOGI(TAG, "knob_hold_angle_milli = %d", knob->hold_angle_milli);

    return ESP_OK;
}

static esp_err_t airmouse_config_parse_space_switch(
    cJSON *root,
    airmouse_space_switch_config_t *space_switch)
{
    int horizontal_axis;
    int left_sign;
    int vertical_axis;
    int up_sign;
    esp_err_t ret;

    if (root == NULL || space_switch == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    horizontal_axis = space_switch->horizontal_axis;
    left_sign = space_switch->left_sign;
    vertical_axis = space_switch->vertical_axis;
    up_sign = space_switch->up_sign;

    ret = json_get_optional_axis3(root,
                                  "space_switch_horizontal_axis",
                                  horizontal_axis,
                                  &horizontal_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "space_switch_left_sign",
                                left_sign,
                                &left_sign);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_axis3(root,
                                  "space_switch_vertical_axis",
                                  vertical_axis,
                                  &vertical_axis);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "space_switch_up_sign",
                                up_sign,
                                &up_sign);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = validate_space_switch_config(horizontal_axis,
                                       left_sign,
                                       vertical_axis,
                                       up_sign);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "invalid space switch config: horizontal_axis=%d, left_sign=%d, vertical_axis=%d, up_sign=%d",
                 horizontal_axis,
                 left_sign,
                 vertical_axis,
                 up_sign);
        return ret;
    }

    space_switch->horizontal_axis = horizontal_axis;
    space_switch->left_sign = left_sign;
    space_switch->vertical_axis = vertical_axis;
    space_switch->up_sign = up_sign;

    ESP_LOGI(TAG, "space_switch_horizontal_axis = %s",
             axis3_to_string(space_switch->horizontal_axis));
    ESP_LOGI(TAG, "space_switch_left_sign = %d", space_switch->left_sign);
    ESP_LOGI(TAG, "space_switch_vertical_axis = %s",
             axis3_to_string(space_switch->vertical_axis));
    ESP_LOGI(TAG, "space_switch_up_sign = %d", space_switch->up_sign);

    return ESP_OK;
}

static esp_err_t airmouse_config_parse_inference(
    cJSON *root,
    airmouse_inference_config_t *inference)
{
    bool enable;
    int sample_interval_ms;
    esp_err_t ret;

    if (root == NULL || inference == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    enable = inference->enable;
    sample_interval_ms = inference->sample_interval_ms;

    ret = json_get_optional_bool(root, "infer_enable", enable, &enable);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = json_get_optional_int(root,
                                "infer_sample_interval_ms",
                                sample_interval_ms,
                                &sample_interval_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    if (enable && sample_interval_ms <= 0) {
        ESP_LOGE(TAG,
                 "invalid inference config: infer_enable=%d, infer_sample_interval_ms=%d",
                 enable,
                 sample_interval_ms);
        return ESP_ERR_INVALID_ARG;
    }

    if (enable &&
            ((sample_interval_ms < CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS) ||
             ((sample_interval_ms % CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS) != 0))) {
        ESP_LOGE(TAG,
                 "invalid inference sample interval: %d ms must be an integer multiple of imu interval %d ms",
                 sample_interval_ms,
                 CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS);
        return ESP_ERR_INVALID_ARG;
    }

#if !CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    if (enable) {
        ESP_LOGE(TAG,
                 "invalid inference config: infer_enable=true but inference is disabled in this firmware");
        return ESP_ERR_INVALID_ARG;
    }
#endif

    inference->enable = enable;
    inference->sample_interval_ms = sample_interval_ms;

    ESP_LOGI(TAG, "infer_enable = %d", inference->enable);
    ESP_LOGI(TAG, "infer_sample_interval_ms = %d",
             inference->sample_interval_ms);

    return ESP_OK;
}

static esp_err_t airmouse_config_add_pose_json(cJSON *root,
                                               const airmouse_pose_config_t *pose)
{
    if (!cJSON_AddStringToObject(root,
                                 "pose_forward_axis",
                                 axis6_to_string(pose->forward_axis)) ||
            !cJSON_AddStringToObject(root,
                                     "pose_up_axis",
                                     axis6_to_string(pose->up_axis)) ||
            !cJSON_AddStringToObject(root,
                                     "pose_screen_x_axis",
                                     axis6_to_string(pose->screen_x_axis)) ||
            !cJSON_AddStringToObject(root,
                                     "pose_screen_y_axis",
                                     axis6_to_string(pose->screen_y_axis)) ||
            !cJSON_AddNumberToObject(root,
                                     "pose_sensitivity_centi",
                                     pose->sensitivity_centi)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t airmouse_config_add_hardware_json(
    cJSON *root,
    const airmouse_hardware_config_t *hardware)
{
    if (!cJSON_AddNumberToObject(root, "imu_i2c_scl", hardware->i2c_scl) ||
            !cJSON_AddNumberToObject(root, "imu_i2c_sda", hardware->i2c_sda) ||
            !cJSON_AddNumberToObject(root, "mag_i2c_scl", hardware->mag_i2c_scl) ||
            !cJSON_AddNumberToObject(root, "mag_i2c_sda", hardware->mag_i2c_sda) ||
            !cJSON_AddBoolToObject(root,
                                   "imu_sdo_gpio_control",
                                   hardware->sdo_gpio_control) ||
            !cJSON_AddNumberToObject(root, "imu_sdo_pin", hardware->sdo_pin) ||
            !cJSON_AddNumberToObject(root, "reset_button", hardware->reset_button)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t airmouse_config_add_knob_json(cJSON *root,
                                               const airmouse_knob_config_t *knob)
{
    if (!cJSON_AddStringToObject(root, "knob_axis", axis3_to_string(knob->axis)) ||
            !cJSON_AddNumberToObject(root, "knob_cw_sign", knob->cw_sign) ||
            !cJSON_AddStringToObject(root,
                                     "knob_atan2_first_axis",
                                     axis3_to_string(knob->atan2_first_axis)) ||
            !cJSON_AddStringToObject(root,
                                     "knob_atan2_second_axis",
                                     axis3_to_string(knob->atan2_second_axis)) ||
            !cJSON_AddNumberToObject(root,
                                     "knob_hold_angle_milli",
                                     knob->hold_angle_milli)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t airmouse_config_add_space_switch_json(
    cJSON *root,
    const airmouse_space_switch_config_t *space_switch)
{
    if (!cJSON_AddStringToObject(root,
                                 "space_switch_horizontal_axis",
                                 axis3_to_string(space_switch->horizontal_axis)) ||
            !cJSON_AddNumberToObject(root,
                                     "space_switch_left_sign",
                                     space_switch->left_sign) ||
            !cJSON_AddStringToObject(root,
                                     "space_switch_vertical_axis",
                                     axis3_to_string(space_switch->vertical_axis)) ||
            !cJSON_AddNumberToObject(root,
                                     "space_switch_up_sign",
                                     space_switch->up_sign)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t airmouse_config_add_inference_json(
    cJSON *root,
    const airmouse_inference_config_t *inference)
{
    if (!cJSON_AddBoolToObject(root, "infer_enable", inference->enable) ||
            !cJSON_AddNumberToObject(root,
                                     "infer_sample_interval_ms",
                                     inference->sample_interval_ms)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t airmouse_config_add_firmware_flags_json(cJSON *root)
{
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_AIRMOUSE_ENABLE_BMM350
    const bool mag_enable = true;
#else
    const bool mag_enable = false;
#endif

    if (!cJSON_AddBoolToObject(root, "mag_enable", mag_enable)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void airmouse_config_set_default(airmouse_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->pose.forward_axis = IMU_QUAT_AXIS_NEG_X;
    config->pose.up_axis = IMU_QUAT_AXIS_POS_Z;
    config->pose.screen_x_axis = IMU_QUAT_AXIS_POS_Y;
    config->pose.screen_y_axis = IMU_QUAT_AXIS_NEG_Z;
    config->pose.sensitivity_centi = airmouse_default_pose_sensitivity_centi();

    config->hardware.i2c_scl = CONFIG_IMU_I2C_SCL;
    config->hardware.i2c_sda = CONFIG_IMU_I2C_SDA;
#if CONFIG_AIRMOUSE_ENABLE_BMM350
    config->hardware.mag_i2c_scl = CONFIG_MAG_I2C_SCL;
    config->hardware.mag_i2c_sda = CONFIG_MAG_I2C_SDA;
#else
    config->hardware.mag_i2c_scl = CONFIG_IMU_I2C_SCL;
    config->hardware.mag_i2c_sda = CONFIG_IMU_I2C_SDA;
#endif
#if CONFIG_IMU_SDO_GPIO_CONTROL
    config->hardware.sdo_gpio_control = true;
    config->hardware.sdo_pin = CONFIG_IMU_SDO_PIN;
#else
    config->hardware.sdo_gpio_control = false;
    config->hardware.sdo_pin = -1;
#endif
    config->hardware.reset_button = CONFIG_RESET_BUTTON;

    config->knob.axis = CONFIG_AIRMOUSE_GESTURE_KNOB_AXIS;
    config->knob.cw_sign = CONFIG_AIRMOUSE_GESTURE_KNOB_CW_SIGN;
    resolve_default_knob_atan2_axes(config->knob.axis,
                                    &config->knob.atan2_first_axis,
                                    &config->knob.atan2_second_axis);
    config->knob.hold_angle_milli =
        CONFIG_AIRMOUSE_GESTURE_KNOB_STABLE_DPS_MILLI;

    config->space_switch.horizontal_axis =
        CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_HORIZONTAL_AXIS;
    config->space_switch.left_sign =
        CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_LEFT_SIGN;
    config->space_switch.vertical_axis =
        CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_VERTICAL_AXIS;
    config->space_switch.up_sign = CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_UP_SIGN;

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    config->inference.enable = true;
    config->inference.sample_interval_ms =
        CONFIG_AIRMOUSE_GESTURE_SAMPLE_INTERVAL_MS;
#else
    config->inference.enable = false;
    config->inference.sample_interval_ms = 0;
#endif
}

esp_err_t airmouse_config_save(const char *json, airmouse_config_t *config)
{
    esp_err_t ret = ESP_OK;
    cJSON *root = NULL;
    airmouse_config_t next;

    if (json == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_Parse(json);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    next = *config;

    ret = airmouse_config_parse_pose(root, &next.pose);
    if (ret == ESP_OK) {
        ret = airmouse_config_parse_hardware(root, &next.hardware);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_parse_knob(root, &next.knob);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_parse_space_switch(root, &next.space_switch);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_parse_inference(root, &next.inference);
    }

    if (ret == ESP_OK) {
        *config = next;
    }

    cJSON_Delete(root);
    return ret;
}

esp_err_t airmouse_config_export_json(const airmouse_config_t *config,
                                      char **json_str_out)
{
    cJSON *root = NULL;
    char *json = NULL;
    esp_err_t ret = ESP_OK;

    if (config == NULL || json_str_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = airmouse_config_add_pose_json(root, &config->pose);
    if (ret == ESP_OK) {
        ret = airmouse_config_add_hardware_json(root, &config->hardware);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_add_knob_json(root, &config->knob);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_add_space_switch_json(root, &config->space_switch);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_add_inference_json(root, &config->inference);
    }
    if (ret == ESP_OK) {
        ret = airmouse_config_add_firmware_flags_json(root);
    }

    if (ret != ESP_OK) {
        cJSON_Delete(root);
        return ret;
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

    *json_str_out = json;
    return ESP_OK;
}
