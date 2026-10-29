/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_types.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "at581x_reg.h"

/* AT581X address */
#define AT581X_ADDRRES_0 (0x28)

/**
 * @brief Type of AT581X device handle
 *
 */
typedef void *at581x_dev_handle_t;

/**
 * @brief Touch controller interrupt callback type
 *
 */
typedef void (*at581x_interrupt_callback_t)(void *arg);

typedef struct {
    uint16_t self_check_tm_cfg;     /*!< Power-on self-test time, range: 0 ~ 65536 ms */
    uint16_t protect_tm_cfg;        /*!< Protection time, recommended 1000 ms */
    uint16_t trig_base_tm_cfg;      /*!< Default: 500 ms */
    uint16_t trig_keep_tm_cfg;      /*!< Total trig time = TRIGGER_BASE_TIME + DEF_TRIGGER_KEEP_TIME, minimum: 1 */
    uint16_t delta_cfg;             /*!< Delta value: 0 ~ 1023, the larger the value, the shorter the distance */
    uint16_t gain_cfg;              /*!< Default: AT581X_STAGE_GAIN_3*/
    uint16_t power_cfg;             /*!< Refer to "Power consumption configuration table" */
} at581x_default_cfg_t;

/**
 * @brief AT581X I2C config struct
 *
 */
typedef struct {
    i2c_bus_handle_t bus_inst;      /*!< I2C bus instance handle */
    uint8_t i2c_addr;               /*!< I2C address of AT581X device */

    gpio_num_t int_gpio_num;        /*!< GPIO number of interrupt pin */
    unsigned int interrupt_level;   /*!< Active Level of interrupt pin */
    at581x_interrupt_callback_t interrupt_callback;  /*!< User callback called after the touch interrupt occurred */

    const at581x_default_cfg_t *def_conf;
} at581x_i2c_config_t;

/**
 * @brief Register user callback called after the sensor interrupt occurred
 *
 * @param[in] handle: Sensor handler
 * @param callback: Interrupt callback
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t at581x_register_interrupt_callback(at581x_dev_handle_t handle, at581x_interrupt_callback_t callback);

/**
 * @brief Create new AT581X device handle.
 *
 * @param[in]  i2c_conf Config for I2C used by AT581X
 * @param[out] handle_out New AT581X device handle
 * @return
 *          - ESP_OK                  Device handle creation success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 *          - ESP_ERR_NO_MEM          Memory allocation failed.
 */
esp_err_t at581x_new_sensor(const at581x_i2c_config_t *i2c_conf, at581x_dev_handle_t *handle_out);

/**
 * @brief Delete AT581X device handle.
 *
 * @param handle AT581X device handle
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_del_sensor(at581x_dev_handle_t handle);

/**
 * @brief Software reset.
 *
 * @param handle AT581X device handle
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_soft_reset(at581x_dev_handle_t handle);

/**
 * @brief Set the RF transmit frequency point.
 *
 * @param handle AT581X device handle
 * @param[in] freq_0x5f Refer to the frequency configuration table in at581x_reg.h
 * @param[in] freq_0x60 Refer to the frequency configuration table in at581x_reg.h
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_freq_point(at581x_dev_handle_t handle, uint8_t freq_0x5f, uint8_t freq_0x60);

/**
 * @brief Set self_test time after poweron.
 *
 * @note at581x_soft_reset is required after modification.
 *
 * @param handle AT581X device handle
 * @param[in] self_check_time time of self_test, unit: ms
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_self_check_time(at581x_dev_handle_t handle, uint32_t self_check_time);

/**
 * @brief Set the trig base time (minimum 500ms).
 *
 * @param handle AT581X device handle
 * @param[in] base_time trig base time, unit: ms
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_trig_base_time(at581x_dev_handle_t handle, uint64_t base_time);

/**
 * @brief Set the protection time after triggering.
 *
 * @note There is no sensing function within the protection time.
 *
 * @param handle AT581X device handle
 * @param[in] protect_time protection time, unit: ms
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_protect_time(at581x_dev_handle_t handle, uint32_t protect_time);

/**
 * @brief Set detection distance.
 *
 * @param handle AT581X device handle
 * @param[in] pwr_setting power_setting, it's valid for AT5815
 * @param[in] delta Threshold range: 0 ~ 1023
 * @param[in] gain Gain of radar, please adjust according to your radar's sensitivity
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_distance(at581x_dev_handle_t handle, uint8_t pwr_setting, uint32_t delta, at581x_gain_t gain);

/**
 * @brief Set the delay time after the trig.
 *
 * @param handle AT581X device handle
 * @param[in] keep_time delay time, unit: ms
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_trig_keep_time(at581x_dev_handle_t handle, uint64_t keep_time);

/**
 * @brief Set light sensor (no light sensor by default).
 *
 * @param handle AT581X device handle
 * @param[in] onoff on/off the light sensor
 * @param[in] light_sensor_value_high threshold high
 * @param[in] light_sensor_value_low threshold low
 * @param[in] light_sensor_iniverse inverse
 *
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_light_sensor_threshold(at581x_dev_handle_t handle,
                                            bool onoff,
                                            uint32_t light_sensor_value_high,
                                            uint32_t light_sensor_value_low,
                                            uint32_t light_sensor_iniverse);
/**
 * @brief Control the RF module.
 *
 * @note After the RF module is off, it can save about 10uA power consumption.
 *
 * @param handle AT581X device handle
 * @param[in] onoff on/off the RF module
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_rf_onoff(at581x_dev_handle_t handle, bool onoff);

/**
 * @brief Set detect window length & threshold
 *
 * @param handle AT581X device handle
 * @param[in] window_length Number of windows per detection (Default: 4)
 * @param[in] window_threshold Number of windows needed to trig (Default: 3)
 * @return
 *          - ESP_OK                  Device handle deletion success.
 *          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
 */
esp_err_t at581x_set_detect_window(at581x_dev_handle_t handle, uint8_t window_length, uint8_t window_threshold);

/**
 * @brief Radar default configuration structure
 *
 */
#define ATH581X_INITIALIZATION_CONFIG()     \
    {                                       \
        .self_check_tm_cfg = 2000,          \
        .protect_tm_cfg = 1000,             \
        .trig_base_tm_cfg = 500,            \
        .trig_keep_tm_cfg = 1500,           \
        .delta_cfg = 200,                   \
        .gain_cfg = AT581X_STAGE_GAIN_3,    \
        .power_cfg = AT581X_POWER_70uA,     \
    }

#ifdef __cplusplus
}
#endif
