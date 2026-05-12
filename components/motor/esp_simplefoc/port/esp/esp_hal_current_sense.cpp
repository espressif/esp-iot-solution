/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "current_sense/hardware_api.h"
#include "esp_hal_bldc_6pwm.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_private/adc_private.h"
#include "esp_check.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

static const char *TAG = "esp_hal_current_sense";
static constexpr adc_atten_t ADC_ATTENUATION = ADC_ATTEN_DB_12;
static constexpr int ADC_PIN_NOT_SET = static_cast<int>(NOT_SET);

struct EspAdcChannelConfig {
    int pin = ADC_PIN_NOT_SET;
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t channel = ADC_CHANNEL_0;
    adc_cali_handle_t cali_handle = nullptr;
    volatile int raw_value = 0;
};

struct EspInlineCurrentSenseParams {
    int pins[3] = {ADC_PIN_NOT_SET, ADC_PIN_NOT_SET, ADC_PIN_NOT_SET};
    EspAdcChannelConfig channels[3];
};

struct EspLowsideCurrentSenseParams {
    int pins[3] = {ADC_PIN_NOT_SET, ADC_PIN_NOT_SET, ADC_PIN_NOT_SET};
    EspAdcChannelConfig channels[3];
    int num_channels = 0;
    EspMcpwmDriverParams *driver_params = nullptr;
    volatile uint32_t last_timer_event_count = 0;
};

static adc_oneshot_unit_handle_t s_adc_unit_handles[SOC_ADC_PERIPH_NUM] = {};
static EspLowsideCurrentSenseParams *s_active_lowside_params = nullptr;

static int adc_unit_to_index(adc_unit_t unit)
{
    switch (unit) {
    case ADC_UNIT_1:
        return 0;
#if SOC_ADC_PERIPH_NUM >= 2
    case ADC_UNIT_2:
        return 1;
#endif
    default:
        return -1;
    }
}

static adc_oneshot_unit_handle_t get_existing_adc_unit_handle(adc_unit_t unit)
{
    const int unit_index = adc_unit_to_index(unit);
    if (unit_index < 0 || unit_index >= SOC_ADC_PERIPH_NUM) {
        return nullptr;
    }

    return s_adc_unit_handles[unit_index];
}

static adc_oneshot_unit_handle_t get_adc_unit_handle(adc_unit_t unit)
{
    const int unit_index = adc_unit_to_index(unit);
    if (unit_index < 0 || unit_index >= SOC_ADC_PERIPH_NUM) {
        return nullptr;
    }

    if (s_adc_unit_handles[unit_index] == nullptr) {
        adc_oneshot_unit_init_cfg_t init_config = {};
        init_config.unit_id = unit;
        if (adc_oneshot_new_unit(&init_config, &s_adc_unit_handles[unit_index]) != ESP_OK) {
            ESP_LOGE(TAG, "failed to create ADC unit handle for ADC%d", unit);
            return nullptr;
        }
    }

    return s_adc_unit_handles[unit_index];
}

static adc_cali_handle_t create_adc_cali_handle(adc_unit_t unit, adc_channel_t channel)
{
    adc_cali_handle_t handle = nullptr;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {};
    cali_config.unit_id = unit;
    cali_config.chan = channel;
    cali_config.atten = ADC_ATTENUATION;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (err == ESP_OK) {
        return handle;
    }
    ESP_LOGW(TAG, "curve fitting calibration unavailable for ADC%d channel %d: %s", unit, channel, esp_err_to_name(err));
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {};
    cali_config.unit_id = unit;
    cali_config.atten = ADC_ATTENUATION;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    esp_err_t err = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (err == ESP_OK) {
        return handle;
    }
    ESP_LOGW(TAG, "line fitting calibration unavailable for ADC%d channel %d: %s", unit, channel, esp_err_to_name(err));
#endif

    return nullptr;
}

static EspAdcChannelConfig *find_adc_channel_config(const int pin, EspInlineCurrentSenseParams *params)
{
    for (int i = 0; i < 3; i++) {
        if (params->channels[i].pin == pin) {
            return &params->channels[i];
        }
    }

    return nullptr;
}

static EspAdcChannelConfig *find_adc_channel_config(const int pin, EspLowsideCurrentSenseParams *params)
{
    for (int i = 0; i < 3; i++) {
        if (params->channels[i].pin == pin) {
            return &params->channels[i];
        }
    }

    return nullptr;
}

static bool configure_adc_channel(const int pin, EspAdcChannelConfig *channel_config, bool require_adc1)
{
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t channel = ADC_CHANNEL_0;
    if (adc_oneshot_io_to_channel(pin, &unit, &channel) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d is not a valid ADC pin", pin);
        return false;
    }
    if (require_adc1 && unit != ADC_UNIT_1) {
        ESP_LOGE(TAG, "GPIO%d maps to ADC%d, lowside current sense only supports ADC1", pin, unit);
        return false;
    }

    adc_oneshot_unit_handle_t adc_handle = get_adc_unit_handle(unit);
    if (adc_handle == nullptr) {
        ESP_LOGE(TAG, "failed to get ADC%d unit handle for GPIO%d", unit, pin);
        return false;
    }

    adc_oneshot_chan_cfg_t adc_channel_config = {
        .atten = ADC_ATTENUATION,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_config_channel(adc_handle, channel, &adc_channel_config) != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure ADC channel for GPIO%d", pin);
        return false;
    }

    channel_config->pin = pin;
    channel_config->unit = unit;
    channel_config->channel = channel;
    channel_config->cali_handle = create_adc_cali_handle(unit, channel);
    if (channel_config->cali_handle == nullptr) {
        ESP_LOGE(TAG, "failed to create ADC calibration handle for GPIO%d (ADC%d channel %d)", pin, unit, channel);
        return false;
    }

    return true;
}

float _readADCVoltageInline(const int pin, const void *cs_params)
{
    if (cs_params == nullptr) {
        return 0.0f;
    }

    EspInlineCurrentSenseParams *params = (EspInlineCurrentSenseParams *)cs_params;
    EspAdcChannelConfig *channel_config = find_adc_channel_config(pin, params);
    if (channel_config == nullptr) {
        return 0.0f;
    }

    adc_oneshot_unit_handle_t adc_handle = get_adc_unit_handle(channel_config->unit);
    if (adc_handle == nullptr) {
        return 0.0f;
    }

    int voltage_mv = 0;
    if (channel_config->cali_handle == nullptr) {
        return 0.0f;
    }
    if (adc_oneshot_get_calibrated_result(adc_handle, channel_config->cali_handle, channel_config->channel, &voltage_mv) != ESP_OK) {
        return 0.0f;
    }

    // The SimpleFOC current sense layer expects voltage in volts, while ESP ADC calibration returns mV.
    return voltage_mv / 1000.0f;
}

void *_configureADCInline(const void *driver_params, const int pinA, const int pinB, const int pinC)
{
    _UNUSED(driver_params);

    EspInlineCurrentSenseParams *params = new EspInlineCurrentSenseParams;
    params->pins[0] = pinA;
    params->pins[1] = pinB;
    params->pins[2] = pinC;

    const int pins[3] = {pinA, pinB, pinC};
    for (int i = 0; i < 3; i++) {
        if (!_isset(pins[i])) {
            continue;
        }
        if (!configure_adc_channel(pins[i], &params->channels[i], false)) {
            delete params;
            return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
        }
    }

    ESP_LOGI(TAG, "inline current sense configured on GPIOs: A=%d B=%d C=%d", pinA, pinB, pinC);
    return params;
}

void *_configureADCLowSide(const void *driver_params, const int pinA, const int pinB, const int pinC)
{
    if (driver_params == nullptr) {
        ESP_LOGE(TAG, "lowside current sense requires MCPWM driver params");
        return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
    }

    EspMcpwmDriverParams *mcpwm_driver_params = (EspMcpwmDriverParams *)driver_params;

    EspLowsideCurrentSenseParams *params = new EspLowsideCurrentSenseParams;
    params->pins[0] = pinA;
    params->pins[1] = pinB;
    params->pins[2] = pinC;
    params->driver_params = mcpwm_driver_params;

    const int pins[3] = {pinA, pinB, pinC};
    for (int i = 0; i < 3; i++) {
        if (!_isset(pins[i])) {
            continue;
        }
        if (!configure_adc_channel(pins[i], &params->channels[i], true)) {
            delete params;
            return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
        }
        params->num_channels++;
    }

    ESP_LOGI(TAG, "lowside current sense configured on GPIOs: A=%d B=%d C=%d", pinA, pinB, pinC);
    return params;
}

void _startADC3PinConversionLowSide()
{
    if (s_active_lowside_params == nullptr) {
        return;
    }

    // Best-effort sync to a fresh PWM full event. Avoid blocking indefinitely if the event
    // has already happened or interrupts are momentarily delayed.
    uint32_t start_event_count = s_active_lowside_params->last_timer_event_count;
    const uint32_t wait_start_us = micros();
    while (s_active_lowside_params->last_timer_event_count == start_event_count) {
        if (micros() - wait_start_us > 200) {
            break;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (!_isset(s_active_lowside_params->channels[i].pin)) {
            continue;
        }

        adc_oneshot_unit_handle_t adc_handle = get_existing_adc_unit_handle(s_active_lowside_params->channels[i].unit);
        if (adc_handle == nullptr) {
            continue;
        }

        int raw = 0;
        if (adc_oneshot_read(adc_handle, s_active_lowside_params->channels[i].channel, &raw) == ESP_OK) {
            s_active_lowside_params->channels[i].raw_value = raw;
        }
    }
}

float _readADCVoltageLowSide(const int pin, const void *cs_params)
{
    if (cs_params == nullptr) {
        return 0.0f;
    }

    EspLowsideCurrentSenseParams *params = (EspLowsideCurrentSenseParams *)cs_params;
    EspAdcChannelConfig *channel_config = find_adc_channel_config(pin, params);
    if (channel_config == nullptr || channel_config->cali_handle == nullptr) {
        return 0.0f;
    }

    int voltage_mv = 0;
    if (adc_cali_raw_to_voltage(channel_config->cali_handle, channel_config->raw_value, &voltage_mv) != ESP_OK) {
        return 0.0f;
    }

    return voltage_mv / 1000.0f;
}

void _driverSyncLowSide(void *driver_params, void *cs_params)
{
    if (driver_params == nullptr || cs_params == nullptr) {
        return;
    }

    EspMcpwmDriverParams *mcpwm_driver_params = (EspMcpwmDriverParams *)driver_params;
    mcpwm_driver_params->lowside_cs_params = cs_params;
    s_active_lowside_params = (EspLowsideCurrentSenseParams *)cs_params;
}

bool IRAM_ATTR esp_simplefoc_lowside_on_mcpwm_full(mcpwm_timer_handle_t timer,
                                                   const mcpwm_timer_event_data_t *edata,
                                                   void *user_data)
{
    _UNUSED(timer);
    _UNUSED(edata);

    if (user_data == nullptr) {
        return false;
    }

    EspMcpwmDriverParams *driver_params = (EspMcpwmDriverParams *)user_data;
    if (driver_params->lowside_cs_params == nullptr) {
        return false;
    }

    EspLowsideCurrentSenseParams *params = (EspLowsideCurrentSenseParams *)driver_params->lowside_cs_params;
    params->last_timer_event_count = params->last_timer_event_count + 1;

    return false;
}
