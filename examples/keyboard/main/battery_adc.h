/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief Initialize battery ADC.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: Out of memory.
 */
esp_err_t battery_adc_init(void);

#ifdef __cpluscpls
}
#endif
