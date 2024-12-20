/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POWER_MEASURE__
#define __POWER_MEASURE__

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#include <esp_event.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief STORGE_FACTOR_KEY
 */
#define STORGE_FACTOR_KEY "factor"

/**
 * @brief POWER_MEASURE event base
 */
/** @cond **/
ESP_EVENT_DECLARE_BASE(POWER_MEASURE_EVENT);
/** @endcond **/

/**
 * @brief Enumeration of power measurement events.
 */
typedef enum {
    POWER_MEASURE_INIT_DONE = 1,    /*!< Initialisation Done */
    POWER_MEASURE_OVERCURRENT,      /*!< Overcurrent detected */
    POWER_MEASURE_OVERVOLTAGE,      /*!< Overvoltage detected */
    POWER_MEASURE_UNDERVOLTAGE,     /*!< Undervoltage */
    POWER_MEASURE_ENERGY_REPORT,    /*!< report energy */
    POWER_MEASURE_HOME_APPLIANCES_ONLINE,   /*!< appliances online */
    POWER_MEASURE_HOME_APPLIANCES_OFFLINE,  /*!< appliances offline */
} power_measure_event_t;

/**
 * @brief Enumeration of rated voltage levels.
 */
typedef enum {
    RATED_VOLTAGE_110V = 110,       /*!< Rated voltage of 110 volts */
    RATED_VOLTAGE_120V = 120,       /*!< Rated voltage of 120 volts */
    RATED_VOLTAGE_220V = 220,       /*!< Rated voltage of 220 volts */
} power_measure_rated_voltage_t;

/**
 * @brief Enumeration of supported chip types.
 */
typedef enum {
    CHIP_BL0937 = 0,                 /*!< BL0937 chip type */
    CHIP_MAX,                        /*!< Maximum chip type */
} power_measure_chip_t;

/**
 * @brief Data structure for overcurrent event data.
 */
typedef struct {
    float current_value;             /*!< Current value measured during the overcurrent event */
    uint16_t trigger_value;          /*!< Trigger threshold for overcurrent detection */
} overcurrent_event_data_t;

/**
 * @brief Data structure for overvoltage event data.
 */
typedef struct {
    float voltage_value;             /*!< Voltage value measured during the overvoltage event */
    uint16_t overvalue;              /*!< Threshold value for overvoltage detection */
} overvoltage_event_data_t;

/**
 * @brief Data structure for undervoltage event data.
 */
typedef struct {
    float voltage_value;             /*!< Voltage value measured during the undervoltage event */
    uint16_t undervalue;             /*!< Threshold value for undervoltage detection */
} undervoltage_event_data_t;

/**
 * @brief Structure for calibration parameters.
 */
typedef struct {
    float standard_power;            /*!< Standard power in watts */
    float standard_voltage;          /*!< Standard voltage in volts */
    float standard_current;          /*!< Standard current in amperes */
} calibration_parameter_t;

/**
 * @brief Structure for calibration factors.
 */
typedef struct {
    float ki;                        /*!< Calibration factor for current */
    float ku;                        /*!< Calibration factor for voltage */
    float kp;                        /*!< Calibration factor for power */
} calibration_factor_t;

/**
 * @brief Configuration structure for the chip.
 */
typedef struct {
    power_measure_chip_t type;      /*!< Type of the chip used */
    calibration_factor_t factor;     /*!< Calibration factors for the chip */
    gpio_num_t sel_gpio;            /*!< GPIO number for selection */
    gpio_num_t cf1_gpio;            /*!< GPIO number for CF1 */
    gpio_num_t cf_gpio;             /*!< GPIO number for CF */
    uint8_t pin_mode;               /*!< Pin mode configuration */
    float sampling_resistor;        /*!< Value of the sampling resistor */
    float divider_resistor;         /*!< Value of the divider resistor */
} chip_config_t;

/**
 * @brief Initialization configuration structure for power measurement.
 */
typedef struct {
    chip_config_t chip_config;      /*!< Configuration for the chip */
    uint16_t overcurrent;            /*!< Overcurrent threshold */
    uint16_t overvoltage;            /*!< Overvoltage threshold */
    uint16_t undervoltage;           /*!< Undervoltage threshold */
    bool enable_energy_detection;     /*!< Flag to enable energy detection */
} power_measure_init_config_t;

/**
 * @brief power measure component int
 *
 * @param config configuration for power measure hardware
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_NO_MEM
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_init(power_measure_init_config_t* config);

/**
 * @brief power measure component deint
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t power_measure_deinit();

/**
 * @brief get current vltage (V)
 *
 * @param voltage current voltage (V)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_get_voltage(float* voltage);

/**
 * @brief get current current (A)
 *
 * @param current current current (A)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_get_current(float* current);

/**
 * @brief get current active power (W)
 *
 * @param active_power current active power (W)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_get_active_power(float* active_power);

/**
 * @brief get current power factor
 *
 * @param power_factor power factor
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_get_power_factor(float* power_factor);

/**
 * @brief get current energy (Kw/h)
 *
 * @param energy Power consumed by the load (Kw/h)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_get_energy(float* energy);

/**
 * @brief start the energy calculation
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_start_energy_calculation();

/**
 * @brief stop the energy calculation
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_stop_energy_calculation();

/**
 * @brief reset the energy accumulated value in flash
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_reset_energy_calculation();

/**
 * @brief start calibration for factory test
 *
 * @param para calibration parameters
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_start_calibration(calibration_parameter_t* para);

/**
 * @brief get calibration factor from flash
 *
 * @param factor calibration factor
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_STATE
 */
esp_err_t power_measure_get_calibration_factor(calibration_factor_t* factor);

/**
 * @brief reset factor in flash
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t power_measure_calibration_factor_reset(void);

#ifdef __cplusplus
}
#endif

#endif /**< __POWER_MEASURE__ */
