/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "power_measure.h"

/**
 * @brief Internal voltage reference value
 */
#define V_REF_BL0937         1.218

/**
 * @brief The factor of a 1mOhm resistor that allows a ~30A max measurement
 */
#define R_CURRENT_BL0937           (0.002)

/**
 * @brief This is the factor of a voltage divider of 6x 470K upstream and 1k downstream
 */
#define R_VOLTAGE_BL0937        ((2000) + 1)

/**
 * @brief Frequency of the bl0937 internal clock
 */
#define F_OSC_BL0937           (2000000)

/**
 * @brief This value is purely experimental
 *        Higher values allow for a better precision but reduce sampling rate
 *        Lower values increase sampling rate but reduce precision
 *        Values below 0.5s are not recommended since current and voltage output will have no time to stabilise
 */
#define PULSE_TIMEOUT_US       (1000000l)

/**
 * @brief CF1 mode
 */
typedef enum {
    MODE_CURRENT = 0,   /*!< CURRENT MODE */
    MODE_VOLTAGE    /*!< VOLTAGE MODE */
} bl0937_mode_t;

esp_err_t bl0937_init(chip_config_t config);
float bl0937_get_energy();
float bl0937_get_voltage();
float bl0937_get_current();
float bl0937_get_active_power();
float bl0937_get_power_factor();

void bl0937_multiplier_init();
void bl0937_expected_voltage(float value);
void bl0937_expected_current(float value);
void bl0937_expected_active_power(float value);

float bl0937_get_current_multiplier();
float bl0937_get_voltage_multiplier();
float bl0937_get_power_multiplier();

void bl0937_set_current_multiplier(float current_multiplier);
void bl0937_set_voltage_multiplier(float voltage_multiplier);
void bl0937_set_power_multiplier(float power_multiplier);
void reset_energe(void);
