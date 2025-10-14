/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "power_measure.h"

/**
 * @brief Internal voltage reference value
 */
#define V_REF_BL0937         (CONFIG_BL0937_VREF_MV / 1000.0f)

/**
 * @brief The factor of a 1mOhm resistor that allows a ~30A max measurement
 */
#define R_CURRENT_BL0937     (CONFIG_BL0937_R_CURRENT_MOHM / 1000.0f)

/**
 * @brief This is the factor of a voltage divider
 */
#define R_VOLTAGE_BL0937     CONFIG_BL0937_R_VOLTAGE

/**
 * @brief Frequency of the bl0937 internal clock
 */
#define F_OSC_BL0937         CONFIG_BL0937_F_OSC

/**
 * @brief This value is purely experimental
 *        Higher values allow for a better precision but reduce sampling rate
 *        Lower values increase sampling rate but reduce precision
 *        Values below 0.5s are not recommended since current and voltage output will have no time to stabilise
 */
#define PULSE_TIMEOUT_US     CONFIG_BL0937_PULSE_TIMEOUT_US

/**
 * @brief CF1 mode
 */
typedef enum {
    MODE_CURRENT = 0,   /*!< CURRENT MODE */
    MODE_VOLTAGE    /*!< VOLTAGE MODE */
} bl0937_mode_t;
/**
 * @brief Structure for calibration factors.
 */
typedef struct {
    float ki;                        /*!< Calibration factor for current */
    float ku;                        /*!< Calibration factor for voltage */
    float kp;                        /*!< Calibration factor for power */
} calibration_factor_t;

/**
 * @brief Configuration structure for the chip
 */
typedef struct {
    calibration_factor_t factor;     /*!< Calibration factors for the chip */
    gpio_num_t sel_gpio;            /*!< GPIO number for selection */
    gpio_num_t cf1_gpio;            /*!< GPIO number for CF1 */
    gpio_num_t cf_gpio;             /*!< GPIO number for CF */
    uint8_t pin_mode;               /*!< Pin mode configuration */
    float sampling_resistor;        /*!< Value of the sampling resistor */
    float divider_resistor;         /*!< Value of the divider resistor */
} driver_bl0937_config_t;

/**
 * @brief BL0937 handle type
 */
typedef struct bl0937_dev_t *bl0937_handle_t;

esp_err_t bl0937_create(bl0937_handle_t *handle, driver_bl0937_config_t *config);
esp_err_t bl0937_delete(bl0937_handle_t handle);

float bl0937_get_energy(bl0937_handle_t handle);
float bl0937_get_voltage(bl0937_handle_t handle);
float bl0937_get_current(bl0937_handle_t handle);
float bl0937_get_active_power(bl0937_handle_t handle);
float bl0937_get_power_factor(bl0937_handle_t handle);
float bl0937_get_apparent_power(bl0937_handle_t handle);
void bl0937_reset_multipliers(bl0937_handle_t handle);  // Reset multipliers to default values

void bl0937_multiplier_init(bl0937_handle_t handle);
void bl0937_expected_voltage(bl0937_handle_t handle, float value);
void bl0937_expected_current(bl0937_handle_t handle, float value);
void bl0937_expected_active_power(bl0937_handle_t handle, float value);

float bl0937_get_current_multiplier(bl0937_handle_t handle);
float bl0937_get_voltage_multiplier(bl0937_handle_t handle);
float bl0937_get_power_multiplier(bl0937_handle_t handle);

void bl0937_set_current_multiplier(bl0937_handle_t handle, float current_multiplier);
void bl0937_set_voltage_multiplier(bl0937_handle_t handle, float voltage_multiplier);
void bl0937_set_power_multiplier(bl0937_handle_t handle, float power_multiplier);
void reset_energe(bl0937_handle_t handle);

bl0937_mode_t bl0937_getmode(bl0937_handle_t handle);
bl0937_mode_t bl0937_togglemode(bl0937_handle_t handle);
void bl0937_setmode(bl0937_handle_t handle, bl0937_mode_t mode_type);
void bl0937_checkcfsignal(bl0937_handle_t handle);
void bl0937_checkcf1signal(bl0937_handle_t handle);
