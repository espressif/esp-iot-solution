/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX17048_I2C_ADDR_DEFAULT 0x36

#define MAX17048_VCELL_REG 0x02    /*!< ADC measurement of VCELL. */
#define MAX17048_SOC_REG 0x04      /*!< Battery state of charge. */
#define MAX17048_MODE_REG 0x06     /*!< Initiates quick-start, reports hibernate mode, and enables sleep mode. */
#define MAX17048_VERSION_REG 0x08  /*!< IC production version. */
#define MAX17048_HIBRT_REG 0x0A    /*!< Controls thresholds for entering and exiting hibernate mode. */
#define MAX17048_CONFIG_REG 0x0C   /*!< Compensation to optimize performance, sleep mode, alert indicators, and configuration. */
#define MAX17048_VALERT_REG 0x14   /*!< Configures the VCELL range outside of which alerts are generated. */
#define MAX17048_CRATE_REG 0x16    /*!< Approximate charge or discharge rate of the battery. */
#define MAX17048_VRESET_REG 0x18   /*!< Configures VCELL threshold below which the IC resets itself, ID is a one-time factory-programmable identifier. */
#define MAX17048_CHIPID_REG 0x19   /*!< Register that holds semi-unique chip ID. */
#define MAX17048_STATUS_REG 0x1A   /*!< Indicates overvoltage, undervoltage, SOC change, SOC low, and reset alerts. */
#define MAX17048_CMD_REG 0xFE      /*!< Sends POR command. */

/**
 * @brief Different alert acondition in the status register (0x1A)
 *
 */
typedef enum {
    MAX17048_ALERT_FLAG_RESET_INDICATOR = 0x01, /*!< RESET_INDICATOR is set when the device powers up. */
    MAX17048_ALERT_FLAG_VOLTAGE_HIGH = 0x02,    /*!< VOLTAGE_HIGH is set when VCELL has been above ALRT.VALRTMAX. */
    MAX17048_ALERT_FLAG_VOLTAGE_LOW = 0x04,     /*!< VOLTAGE_LOW is set when VCELL has been below ALRT.VALRTMIN. */
    MAX17048_ALERT_FLAG_VOLTAGE_RESET = 0x08,   /*!< VOLTAGE_RESET is set after the device has been reset if EnVr is set. */
    MAX17048_ALERT_FLAG_SOC_LOW = 0x10,         /*!< SOC_LOW is set when SOC crosses the value in CONFIG.ATHD. */
    MAX17048_ALERT_FLAG_SOC_CHARGE = 0x20,      /*!< SOC_CHARGE is set when SOC changes by at least 1% if CONFIG.ALSC is set. */
} max17048_alert_flag_t;

typedef void *max17048_handle_t;

/**
 * @brief Create a MAX17048 device handle
 *
 * @param bus I2C bus handle used to communicate with the MAX17048
 * @param dev_addr I2C address of the MAX17048 device
 *
 * @return
 *     - A valid max17048_handle_t on success
 *     - NULL if initialization fails
 */
max17048_handle_t max17048_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief Delete and free resources associated with the MAX17048 handle
 *
 * @param sensor Pointer to the MAX17048 handle to be deleted
 *
 * @return
 *     - ESP_OK   Success
 *     - ESP_FAIL Failure
 */
esp_err_t max17048_delete(max17048_handle_t *sensor);

/**
 * @brief Read the IC production version of the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param ic_version Pointer to a variable that will store the production version read from the sensor
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor or ic_version is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_ic_version(max17048_handle_t sensor, uint16_t *ic_version);

/**
 * @brief Read the one-time factory-programmable chip ID from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param chip_id Pointer to a variable that will store the chip ID
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor or chip_id is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_chip_id(max17048_handle_t sensor, uint8_t *chip_id);

/**
 * @brief Perform a software reset of the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL command sent failed
 */
esp_err_t max17048_reset(max17048_handle_t sensor);

/**
 * @brief Read the battery cell voltage from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param voltage Pointer to a float variable that will store the cell voltage (in volts)
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor or voltage is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_cell_voltage(max17048_handle_t sensor, float *voltage);

/**
 * @brief Read the battery cell percentage from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param percent Pointer to a float variable that will store the cell percentage
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor or percent is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_cell_percent(max17048_handle_t sensor, float *percent);

/**
 * @brief Read the approximate battery charge or discharge rate from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param rate Pointer to a float variable that will store the approximate charge or discharge rate
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor or rate is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_charge_rate(max17048_handle_t sensor, float *rate);

/**
 * @brief Read the minimum and maximum voltage alert thresholds from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param minv Pointer to a float variable that will store the minimum voltage alert threshold (VALRT.MIN)
 * @param maxv Pointer to a float variable that will store the maximum voltage alert threshold (VALRT.MAX)
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor, minv, or maxv is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_alert_voltage(max17048_handle_t sensor, float *minv, float *maxv);

/**
 * @brief Set the minimum and maximum voltage alert thresholds on the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param minv Minimum voltage alert threshold (in volts)
 * @param maxv Maximum voltage alert threshold (in volts)
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_set_alert_volatge(max17048_handle_t sensor, float minv, float maxv);

/**
 * @brief Read the current alert flag from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param flag Pointer to a variable where the alert flag will be stored
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor is NULL or flag is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_alert_flag(max17048_handle_t sensor, uint8_t *flag);

/**
 * @brief Clear the specified alert flag on the MAX17048 sensor, refer to the 0x1A register in the MAX17048 datasheet
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param flag The alert flag to clear, representing a specific condition
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor is NULL or if the provided flag is invalid
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_clear_alert_flag(max17048_handle_t sensor, max17048_alert_flag_t flag);

/**
 * @brief Clear alert status bit. The alert pin remains logic-low until the system software writes CONFIG.ALRT = 0 to clear the alert
 *
 * @param sensor Handle to the MAX17048 sensor
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_clear_alert_status_bit(max17048_handle_t sensor);

/**
 * @brief Read the hibernation threshold from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param threshold Pointer to a float variable where the hibernation threshold (in %/hr) will be stored
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor is NULL or threshold pointer is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_get_hibernation_threshold(max17048_handle_t sensor, float *threshold);

/**
 * @brief Set the hibernation threshold (HibThr) on the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param threshold The desired hibernation threshold (in %/hr)
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if sensor is NULL
 *     - Other error codes based on I2C communication failure
 */
esp_err_t max17048_set_hibernation_threshold(max17048_handle_t sensor, float threshold);

/**
 * @brief Read the active threshold (ActThr) from the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param threshold Pointer to a float variable where the threshold value (in volts) will be stored
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle or threshold pointer is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_get_active_threshold(max17048_handle_t sensor, float *threshold);

/**
 * @brief Set the active threshold (ActThr) on the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param threshold The desired active threshold in volts
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_set_active_threshold(max17048_handle_t sensor, float threshold);

/**
 * @brief Put the MAX17048 device into hibernation mode
 *
 * @param sensor Handle to the initialized MAX17048 sensor
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_enter_hibernation_mode(max17048_handle_t sensor);

/**
 * @brief Exit hibernation mode on the MAX17048 device
 *
 * @param sensor Handle to the initialized MAX17048 sensor
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_exit_hibernation_mode(max17048_handle_t sensor);

/**
 * @brief Check whether the MAX17048 device is currently in hibernation mode
 *
 * @param sensor Handle to the initialized MAX17048 sensor
 * @param is_hibernate Pointer to a boolean that will be set to true if the device is in hibernation mode, or false otherwise.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL or is_hibernate pointer is NULL.
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction.
 */
esp_err_t max17048_is_hibernate(max17048_handle_t sensor, bool *is_hibernate);

/**
 * @brief Read the voltage that the MAX17048 device considers 'resetting'
 *
 * @param sensor Handle to the initialized MAX17048 sensor
 * @param voltage Pointer to a float variable where the voltage value (in volts) will be stored
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL or voltage pointer is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_get_reset_voltage(max17048_handle_t sensor, float *voltage);

/**
 * @brief Set the voltage that the MAX17048 device considers 'resetting'
 *
 * @param sensor Handle to the initialized MAX17048 sensor
 * @param voltage Below the set voltage value will be considered as reset
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_set_reset_voltage(max17048_handle_t sensor, float voltage);

/**
 * @brief Enable or disable the voltage-reset alert in the MAX17048 sensor
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param enabled Boolean flag indicating whether the voltage-reset alert should be enabled (true) or disabled (false)
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *     - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_set_reset_voltage_alert_enabled(max17048_handle_t sensor, bool enabled);

/**
 * @brief Check if the voltage-reset alert in the MAX17048 sensor is enabled
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param enabled Pointer to a boolean that will store whether the voltage-reset alert is enabled (true) or disabled (false)
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *     - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_is_reset_voltage_alert_enabled(max17048_handle_t sensor, bool *enabled);

/**
 * @brief Configure temperature compensation on the MAX17048 device
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param temperature Temperature value (in degrees Celsius) for compensation
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_temperature_compensation(max17048_handle_t sensor, float temperature);

/**
 * @brief Enable or disable the MAX17048 sleep mode
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param enabled If true, sleep mode is enabled; otherwise, it is disabled
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_set_sleep_enabled(max17048_handle_t sensor, bool enabled);

/**
 * @brief Set the MAX17048 device sleep mode
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param sleep If true, the device enters sleep mode; otherwise, it exits sleep mode
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_set_sleep(max17048_handle_t sensor, bool sleep);

/**
 * @brief Check if the MAX17048 device is in sleep mode
 *
 * @param sensor Handle to the MAX17048 sensor
 * @param is_sleeping Pointer to a bool that is set to true if the device is in sleep mode
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG The sensor handle is NULL or is_sleeping is NULL
 *      - Other esp_err_t codes Errors that may occur during I2C transactions or hardware interaction
 */
esp_err_t max17048_is_sleeping(max17048_handle_t sensor, bool *is_sleeping);

#ifdef __cplusplus
} // extern "C"
#endif
