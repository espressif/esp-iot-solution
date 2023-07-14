/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_hal_adc.h"
#include "driver/gpio.h"
#include "esp_hal_mcpwm.h"
#include "soc/adc_periph.h"
#include "esp_private/adc_private.h"
#include <map>
#include "current_sense/hardware_api.h"

int adc_count = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
/**
 * Using IDF 4.4.*
 */

#else
/**
 * Using IDF 5.*
 */
#if CONFIG_IDF_TARGET_ESP32
std::map<int, std::pair<adc_unit_t, adc_channel_t>> pinToAdc = {
    {GPIO_NUM_32, {ADC_UNIT_1, ADC_CHANNEL_4}},
    {GPIO_NUM_33, {ADC_UNIT_1, ADC_CHANNEL_5}},
    {GPIO_NUM_34, {ADC_UNIT_1, ADC_CHANNEL_6}},
    {GPIO_NUM_35, {ADC_UNIT_1, ADC_CHANNEL_7}},

};
#elif CONFIG_IDF_TARGET_ESP32S3

bool mcpwm_isr_handler(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_ctx);

// ESP32-S3 IDF5.*
std::map<int, std::pair<adc_unit_t, adc_channel_t>> pinToAdc = {
    {GPIO_NUM_1, {ADC_UNIT_1, ADC_CHANNEL_0}},
    {GPIO_NUM_2, {ADC_UNIT_1, ADC_CHANNEL_1}},
    {GPIO_NUM_3, {ADC_UNIT_1, ADC_CHANNEL_2}},
    {GPIO_NUM_4, {ADC_UNIT_1, ADC_CHANNEL_3}},
    {GPIO_NUM_5, {ADC_UNIT_1, ADC_CHANNEL_4}},
    {GPIO_NUM_6, {ADC_UNIT_1, ADC_CHANNEL_5}},
    {GPIO_NUM_7, {ADC_UNIT_1, ADC_CHANNEL_6}},
    {GPIO_NUM_8, {ADC_UNIT_1, ADC_CHANNEL_7}},
};

/**
 * @description: ESP32-S3 calibrates using curve fitting.
 * @param {adc_unit_t} unit
 * @param {adc_channel_t} channel
 * @param {adc_atten_t} atten
 * @param {adc_cali_handle_t} *out_handle
 * @return {*}
 */
bool _adcCalibrationInit(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    printf("calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);

    if (ret == ESP_OK)
    {
        calibrated = true;
    }

    *out_handle = handle;

    return calibrated;
}

/**
 * @description: ESP32-S3 config lowside adc.
 * @param {void} *driver_params
 * @param {int} pinA
 * @param {int} pinB
 * @param {int} pinC
 * @return {*}
 */
void *_configureADCLowSide(const void *driver_params, const int pinA, const int pinB, const int pinC)
{
    if (!pinToAdc.count(pinA) || !pinToAdc.count(pinB) || !pinToAdc.count(pinC))
    {
        printf("GPIO[%d] or GPIO[%d] or GPIO[%d] don't support ADC\n", pinA, pinB, pinC);
        return NULL;
    }

    ESP32MCPWMCurrentSenseParams *params = new ESP32MCPWMCurrentSenseParams;
    int pins[3] = {pinA, pinB, pinC};

    // adc_unit_1 init
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &params->adc_handle));

    // ADC_UNIT_1 Config
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11, // 11 dB attenuation (ADC_ATTEN_DB_11) between 150 to 2450 mV
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    for (int i = 0; i < 3; i++)
    {
        params->pins[i] = pins[i];                                                                          // copy pins to adc params
        ESP_ERROR_CHECK(adc_oneshot_config_channel(params->adc_handle, pinToAdc[pins[i]].second, &config)); // config channel
        if (_adcCalibrationInit(pinToAdc[pins[i]].first, pinToAdc[pins[i]].second, ADC_ATTEN_DB_11, &params->adc_cli_handle[i]))
        {
            printf("GPIO[%d] Channel[%d] calibrates successfully\n", pins[i], pinToAdc[pins[i]].second);
            params->adc_calibrated[i] = true;
        }
        else
        {
            params->adc_calibrated[i] = false;
        }
    }

    params->is_config = true;

    return params;
}

/**
 * @description: Test adc read function.
 * @param {void} *params
 * @return {*}
 */
void _readAdcTest(void *params)
{
    int adc_raw[3] = {0}, voltage[3] = {0};
    ESP32MCPWMCurrentSenseParams *param = (ESP32MCPWMCurrentSenseParams *)(params);

    for (int i = 0; i < 3; i++)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(param->adc_handle, pinToAdc[param->pins[i]].second, &adc_raw[i]));
        if (param->adc_calibrated[i])
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(param->adc_cli_handle[i], adc_raw[i], &voltage[i]));
            printf("IO:%d,raw:%d,conv:%d\n", param->pins[i], adc_raw[i], voltage[i]);
        }
    }
    printf("\n");
}

/**
 * @description: Read Voltage, just take voltage from buffer.
 * @param {int} pinA
 * @param {void} *cs_params
 * @return {*}
 */
float _readADCVoltageLowSide(const int pinA, const void *cs_params)
{
    ESP32MCPWMCurrentSenseParams *params = (ESP32MCPWMCurrentSenseParams *)(cs_params);
    for (int i = 0; i < 3; i++)
    {
        if (pinA == params->pins[i])
        {
            return (params->adc_voltage_conv[i] / 1000.0f); // conver mv to v.
        }
    }
    return 0.0f;
}

/**
 * @description: empty function
 * @return {*}
 */
void _startADC3PinConversionLowSide()
{
}

/**
 * @description: ISR function is registered in mcpwm or ledc driver.
 * @param {void} *driver_params
 * @param {void} *cs_params
 * @return {*}
 */
void _driverSyncLowSide(void *driver_params, void *cs_params)
{
    // mcpwm isr function must register before mcpwm_timer_enable,but we don't know when timer enable
    // so if we use adc, disable timer firstly, then add register isr function, then start timer.
    ESP32MCPWMDriverParams *driverParams = (ESP32MCPWMDriverParams *)(driver_params);

    if (driverParams->is_config)
    {
        ESP_ERROR_CHECK(mcpwm_timer_disable(driverParams->timer));

        // pass
        const mcpwm_timer_event_callbacks_t mcpwm_timer_cb = {
            .on_full = mcpwm_isr_handler,
            .on_empty = NULL,
            .on_stop = NULL,
        };

        mcpwm_timer_register_event_callbacks(driverParams->timer, &mcpwm_timer_cb, cs_params); // bind isr handler, but no user data

        ESP_ERROR_CHECK(mcpwm_timer_enable(driverParams->timer));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(driverParams->timer, MCPWM_TIMER_START_NO_STOP));
    }
}

/**
 * @description: Mcpwm isr handler entry.
 * @param {mcpwm_timer_handle_t} timer
 * @param {mcpwm_timer_event_data_t} *edata
 * @param {void} *user_ctx
 * @return {*}
 */
bool mcpwm_isr_handler(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_ctx)
{
    ESP32MCPWMCurrentSenseParams *currentParams = (ESP32MCPWMCurrentSenseParams *)(user_ctx);
    static int adcPinIndex = 0;

    if (currentParams->is_config)
    {
        adc_oneshot_read_isr(currentParams->adc_handle, pinToAdc[currentParams->pins[adcPinIndex]].second, &currentParams->adc_raw[adcPinIndex]);
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(currentParams->adc_cli_handle[adcPinIndex], currentParams->adc_raw[adcPinIndex], &currentParams->adc_voltage_conv[adcPinIndex]));

        adc_count++;
        adc_count %= 100000;

        adcPinIndex++;
        adcPinIndex %= 3; // the number of adc pins is three
    }

    return true;
}

#endif

#endif
