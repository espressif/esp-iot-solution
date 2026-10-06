/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_bmi270.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
static const char *TAG = "IMU_BMI270";

static constexpr uint32_t AIRMOUSE_BMI270_I2C_CLK_HZ = 100 * 1000;

void airmouse_bmi270_deinit(airmouse_bmi270_handle_t *handle)
{
    if (handle == nullptr) {
        return;
    }

    if (handle->bmi_handle != nullptr) {
        bmi270_sensor_del(&handle->bmi_handle);
    }

    if (handle->i2c_bus != nullptr) {
        i2c_bus_delete(&handle->i2c_bus);
    }

    free(handle->gyr_data);
    handle->gyr_data = nullptr;
    free(handle);
}

airmouse_bmi270_handle_t *airmouse_bmi270_init(
    const airmouse_hardware_config_t *hardware_config)
{
    if (hardware_config == nullptr) {
        ESP_LOGE(TAG, "hardware_config is null");
        return nullptr;
    }

    airmouse_bmi270_handle_t *handle =
        (airmouse_bmi270_handle_t *)calloc(1, sizeof(airmouse_bmi270_handle_t));
    if (handle == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for imu_bmi270_handle_t");
        airmouse_bmi270_deinit(handle);
        return nullptr;
    }

    // Allocate memory for gyro data
    handle->gyr_data = (float (*)[3])calloc(1, 3 * sizeof(float));
    if (handle->gyr_data == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for gyro data");
        airmouse_bmi270_deinit(handle);
        return nullptr;
    }

    esp_err_t ret = ESP_OK;

    if (hardware_config->sdo_gpio_control) {
        // Configure SDO pin only when the board exposes it to the MCU.
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << hardware_config->sdo_pin);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SDO pin configuration failed: %s", esp_err_to_name(ret));
        }
        gpio_set_level(static_cast<gpio_num_t>(hardware_config->sdo_pin), 0);
        ESP_LOGI(TAG, "Drive SDO GPIO%d low for BMI270 address select",
                 hardware_config->sdo_pin);
    } else {
        ESP_LOGI(TAG, "Skip SDO GPIO control because board wiring fixes the address select level");
    }

    // Initialize I2C bus
    ESP_LOGI(TAG, "BMI270 I2C pins: SDA=GPIO%d, SCL=GPIO%d",
             hardware_config->i2c_sda, hardware_config->i2c_scl);
    i2c_config_t i2c_bus_conf = {};
    i2c_bus_conf.mode = I2C_MODE_MASTER;
    i2c_bus_conf.sda_io_num = hardware_config->i2c_sda;
    i2c_bus_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_bus_conf.scl_io_num = hardware_config->i2c_scl;
    i2c_bus_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_bus_conf.master.clk_speed = AIRMOUSE_BMI270_I2C_CLK_HZ;
    handle->i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_bus_conf);
    if (handle->i2c_bus == nullptr) {
        ESP_LOGE(TAG, "Failed to create I2C bus");
        airmouse_bmi270_deinit(handle);
        return nullptr;
    }

    // Initialize BMI270
    ret = bmi270_sensor_create(handle->i2c_bus, &handle->bmi_handle, bmi270_config_file, 0);
    if (ret != ESP_OK || handle->bmi_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to create BMI270 sensor");
        airmouse_bmi270_deinit(handle);
        return nullptr;
    }

    // Configure BMI270
    struct bmi2_sens_config config[2];
    config[BMI2_ACCEL].type = BMI2_ACCEL;
    config[BMI2_GYRO].type = BMI2_GYRO;

    int8_t rslt = bmi2_get_sensor_config(config, 2, handle->bmi_handle);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        // Acc
        config[0].cfg.acc.odr         = BMI2_ACC_ODR_400HZ;
        config[0].cfg.acc.range       = BMI2_ACC_RANGE_2G;
        config[0].cfg.acc.bwp         = BMI2_ACC_NORMAL_AVG4;
        config[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        // Gyro
        config[1].cfg.gyr.odr         = BMI2_GYR_ODR_400HZ;
        config[1].cfg.gyr.range       = BMI2_GYR_RANGE_500;
        config[1].cfg.gyr.bwp         = BMI2_GYR_NORMAL_MODE;
        config[1].cfg.gyr.noise_perf  = BMI2_PERF_OPT_MODE; // Prefer the low-noise high-performance mode here.
        config[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        rslt = bmi2_set_sensor_config(config, 2, handle->bmi_handle);
        bmi2_error_codes_print_result(rslt);

        if (rslt == BMI2_OK) {
            /* Accel and Gyro must be enabled after setting configurations */
            uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_GYRO };
            rslt = bmi2_sensor_enable(sens_list, 2, handle->bmi_handle);
            bmi2_error_codes_print_result(rslt);
        }
    }

    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to configure BMI270");
        airmouse_bmi270_deinit(handle);
        return nullptr;
    }

    return handle;
}

float airmouse_bmi270_lsb_to_g(int16_t val, float full_scale_g, uint8_t bit_width)
{
    const float half_scale = (float)((1u << bit_width) / 2u);
    return (full_scale_g / half_scale) * (float)val;
}

float airmouse_bmi270_lsb_to_dps(int16_t val, float full_scale_dps, uint8_t bit_width)
{
    const float half_scale = (float)((1u << bit_width) / 2u);
    return (full_scale_dps / half_scale) * (float)val;
}

esp_err_t airmouse_bmi270_read_imu_once(
    airmouse_bmi270_handle_t *handle,
    float accel_data[3],
    float gyr_data[3])
{
    if (handle == nullptr || handle->bmi_handle == nullptr || accel_data == nullptr || gyr_data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    struct bmi2_sens_data sensor_data;
    int8_t rslt = bmi2_get_sensor_data(&sensor_data, handle->bmi_handle);
    // ESP_LOGI(TAG, "status=0x%02x", sensor_data.status);
    if (rslt != BMI2_OK || !(sensor_data.status & BMI2_DRDY_ACC) || !(sensor_data.status & BMI2_DRDY_GYR)) {
        ESP_LOGE(TAG, "status=0x%02x", sensor_data.status);
        return ESP_ERR_INVALID_STATE;
    }

    accel_data[0] = airmouse_bmi270_lsb_to_g(sensor_data.acc.x, 2.0f, handle->bmi_handle->resolution);
    accel_data[1] = airmouse_bmi270_lsb_to_g(sensor_data.acc.y, 2.0f, handle->bmi_handle->resolution);
    accel_data[2] = airmouse_bmi270_lsb_to_g(sensor_data.acc.z, 2.0f, handle->bmi_handle->resolution);
    gyr_data[0] = airmouse_bmi270_lsb_to_dps(sensor_data.gyr.x, 500.0f, handle->bmi_handle->resolution);
    gyr_data[1] = airmouse_bmi270_lsb_to_dps(sensor_data.gyr.y, 500.0f, handle->bmi_handle->resolution);
    gyr_data[2] = airmouse_bmi270_lsb_to_dps(sensor_data.gyr.z, 500.0f, handle->bmi_handle->resolution);

    return ESP_OK;
}
