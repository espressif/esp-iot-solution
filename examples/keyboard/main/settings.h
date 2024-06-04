/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include "btn_progress.h"
/**
 * @brief data structure for save to nvs
 *
 */
typedef struct {
    btn_report_type_t report_type;
    light_type_t light_type;
} sys_param_t;

/**
 * @brief Read parameters from nvs
 * @return
 *   - ESP_OK: succeed
 *  - others: fail
 */
esp_err_t settings_read_parameter_from_nvs(void);

/**
 * @brief Write parameters to nvs
 * @return
 *   - ESP_OK: succeed
 *  - others: fail
 */
esp_err_t settings_write_parameter_to_nvs(void);

/**
 * @brief Get parameters
 * @return
 *   - sys_param_t: parameters
 */
sys_param_t *settings_get_parameter(void);
