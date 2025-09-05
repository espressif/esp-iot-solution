/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POWER_MEASURE_BL0937__
#define __POWER_MEASURE_BL0937__

#include "power_measure.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BL0937 specific configuration
 */
typedef struct {
    gpio_num_t sel_gpio;            /*!< GPIO number for selection */
    gpio_num_t cf1_gpio;            /*!< GPIO number for CF1 */
    gpio_num_t cf_gpio;             /*!< GPIO number for CF */
    uint8_t pin_mode;               /*!< Pin mode configuration */
    float sampling_resistor;        /*!< Value of the sampling resistor */
    float divider_resistor;         /*!< Value of the divider resistor */
    float ki;                       /*!< Current calibration factor */
    float ku;                       /*!< Voltage calibration factor */
    float kp;                       /*!< Power calibration factor */
} power_measure_bl0937_config_t;

/**
 * @brief Create a new BL0937 power measurement device
 *
 * @param config Common power measure configuration
 * @param bl0937_config BL0937 specific configuration
 * @param ret_handle Pointer to store the created device handle
 * @return
 *     - ESP_OK: Successfully created the device
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 *     - ESP_ERR_INVALID_STATE: Device already initialized
 */
esp_err_t power_measure_new_bl0937_device(const power_measure_config_t *config,
                                          const power_measure_bl0937_config_t *bl0937_config,
                                          power_measure_handle_t *ret_handle);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MEASURE_BL0937__ */
