/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
// IDF4.4.*

#else
// IDF5.*

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/mcpwm_prelude.h"

typedef struct ESP32MCPWMCurrentSenseParams
{
    int pins[3];
    int adc_raw[3];                       // original adc value
    int adc_voltage_conv[3];              // calibrated voltage, [mv]
    adc_oneshot_unit_handle_t adc_handle; // adc handler
    adc_cali_handle_t adc_cli_handle[3];  // adc handlers after calibration
    bool adc_calibrated[3];               // adc calibration
    bool is_config = false;               // wait for isr register
} ESP32MCPWMCurrentSenseParams;

#endif

void *_configureADCLowSide(const void *driver_params, const int pinA, const int pinB, const int pinC);
void _readAdcTest(void *params);

