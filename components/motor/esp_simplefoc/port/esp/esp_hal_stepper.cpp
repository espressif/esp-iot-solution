/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "drivers/StepperDriver2PWM.h"
#include "drivers/StepperDriver4PWM.h"
#include "driver/ledc.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

static const char *TAG = "esp_hal_stepper";

#define ESP_SIMPLEFOC_STEPPER_PWM_FREQUENCY (20 * 1000)
#define ESP_SIMPLEFOC_STEPPER_LEDC_TIMER LEDC_TIMER_0
#define ESP_SIMPLEFOC_STEPPER_LEDC_MODE LEDC_LOW_SPEED_MODE
#define ESP_SIMPLEFOC_STEPPER_LEDC_DUTY_RES LEDC_TIMER_9_BIT
#define ESP_SIMPLEFOC_STEPPER_LEDC_DUTY_MAX (511)

struct EspStepperLedcDriverParams {
    int channels[4] = {-1, -1, -1, -1};
    int num_channels = 0;
};

static int s_next_ledc_channel = SOC_LEDC_CHANNEL_NUM - 1;

static bool allocate_ledc_channels(int num_channels, int *channels)
{
    if (num_channels <= 0 || num_channels > SOC_LEDC_CHANNEL_NUM) {
        return false;
    }

    if (s_next_ledc_channel - (num_channels - 1) < 0) {
        return false;
    }

    for (int i = 0; i < num_channels; i++) {
        channels[i] = s_next_ledc_channel--;
    }

    return true;
}

static void rollback_ledc_channels(int num_channels)
{
    s_next_ledc_channel += num_channels;
    if (s_next_ledc_channel >= SOC_LEDC_CHANNEL_NUM) {
        s_next_ledc_channel = SOC_LEDC_CHANNEL_NUM - 1;
    }
}

static esp_err_t configure_ledc_timer()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = ESP_SIMPLEFOC_STEPPER_LEDC_MODE,
        .duty_resolution = ESP_SIMPLEFOC_STEPPER_LEDC_DUTY_RES,
        .timer_num = ESP_SIMPLEFOC_STEPPER_LEDC_TIMER,
        .freq_hz = ESP_SIMPLEFOC_STEPPER_PWM_FREQUENCY,
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        .clk_cfg = LEDC_AUTO_CLK,
#else
        .clk_cfg = LEDC_USE_APB_CLK,
#endif
        .deconfigure = false,
    };

    return ledc_timer_config(&ledc_timer);
}

static void *configure_stepper_pwm(int num_channels, const int *pins)
{
    EspStepperLedcDriverParams *params = new EspStepperLedcDriverParams;
    params->num_channels = num_channels;

    if (!allocate_ledc_channels(num_channels, params->channels)) {
        ESP_LOGE(TAG, "not enough LEDC channels for %dPWM stepper driver", num_channels);
        delete params;
        return SIMPLEFOC_DRIVER_INIT_FAILED;
    }

    if (configure_ledc_timer() != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure LEDC timer");
        rollback_ledc_channels(num_channels);
        delete params;
        return SIMPLEFOC_DRIVER_INIT_FAILED;
    }

    ledc_channel_config_t ledc_channel = {};
    for (int i = 0; i < num_channels; i++) {
        ledc_channel.speed_mode = ESP_SIMPLEFOC_STEPPER_LEDC_MODE;
        ledc_channel.channel = static_cast<ledc_channel_t>(params->channels[i]);
        ledc_channel.timer_sel = ESP_SIMPLEFOC_STEPPER_LEDC_TIMER;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
        ledc_channel.intr_type = LEDC_INTR_DISABLE;
#endif
        ledc_channel.gpio_num = pins[i];
        ledc_channel.duty = 0;
        ledc_channel.hpoint = 0;
        if (ledc_channel_config(&ledc_channel) != ESP_OK) {
            ESP_LOGE(TAG, "failed to configure LEDC channel %d for GPIO%d", params->channels[i], pins[i]);
            rollback_ledc_channels(num_channels);
            delete params;
            return SIMPLEFOC_DRIVER_INIT_FAILED;
        }
    }

    return params;
}

static void write_stepper_pwm(const float *duty_cycles, int num_channels, void *params)
{
    if (params == nullptr || params == SIMPLEFOC_DRIVER_INIT_FAILED) {
        return;
    }

    EspStepperLedcDriverParams *driver_params = static_cast<EspStepperLedcDriverParams *>(params);
    for (int i = 0; i < num_channels; i++) {
        uint32_t duty = static_cast<uint32_t>(_constrain(ESP_SIMPLEFOC_STEPPER_LEDC_DUTY_MAX * duty_cycles[i], 0.0f, static_cast<float>(ESP_SIMPLEFOC_STEPPER_LEDC_DUTY_MAX)));
        ledc_channel_t channel = static_cast<ledc_channel_t>(driver_params->channels[i]);
        ESP_ERROR_CHECK(ledc_set_duty(ESP_SIMPLEFOC_STEPPER_LEDC_MODE, channel, duty));
        ESP_ERROR_CHECK(ledc_update_duty(ESP_SIMPLEFOC_STEPPER_LEDC_MODE, channel));
    }
}

void *_configure2PWM(long pwm_frequency, const int pinA, const int pinB)
{
    _UNUSED(pwm_frequency);
    const int pins[2] = {pinA, pinB};
    return configure_stepper_pwm(2, pins);
}

void *_configure4PWM(long pwm_frequency, const int pin1A, const int pin1B, const int pin2A, const int pin2B)
{
    _UNUSED(pwm_frequency);
    const int pins[4] = {pin1A, pin1B, pin2A, pin2B};
    return configure_stepper_pwm(4, pins);
}

void _writeDutyCycle2PWM(float dc_a, float dc_b, void *params)
{
    const float duty_cycles[2] = {dc_a, dc_b};
    write_stepper_pwm(duty_cycles, 2, params);
}

void _writeDutyCycle4PWM(float dc_1a, float dc_1b, float dc_2a, float dc_2b, void *params)
{
    const float duty_cycles[4] = {dc_1a, dc_1b, dc_2a, dc_2b};
    write_stepper_pwm(duty_cycles, 4, params);
}

StepperDriver2PWM::StepperDriver2PWM(int _pwm1, int *_in1, int _pwm2, int *_in2, int en1, int en2)
{
    pwm1 = _pwm1;
    dir1a = _in1[0];
    dir1b = _in1[1];
    pwm2 = _pwm2;
    dir2a = _in2[0];
    dir2b = _in2[1];
    enable_pin1 = en1;
    enable_pin2 = en2;
    voltage_power_supply = DEF_POWER_SUPPLY;
    voltage_limit = NOT_SET;
    pwm_frequency = NOT_SET;
}

StepperDriver2PWM::StepperDriver2PWM(int _pwm1, int _dir1, int _pwm2, int _dir2, int en1, int en2)
{
    pwm1 = _pwm1;
    dir1a = _dir1;
    pwm2 = _pwm2;
    dir2a = _dir2;
    dir1b = NOT_SET;
    dir2b = NOT_SET;
    enable_pin1 = en1;
    enable_pin2 = en2;
    voltage_power_supply = DEF_POWER_SUPPLY;
    voltage_limit = NOT_SET;
    pwm_frequency = NOT_SET;
}

void StepperDriver2PWM::enable()
{
    if (_isset(enable_pin1)) {
        digitalWrite(enable_pin1, HIGH);
    }
    if (_isset(enable_pin2)) {
        digitalWrite(enable_pin2, HIGH);
    }
    setPwm(0, 0);
}

void StepperDriver2PWM::disable()
{
    setPwm(0, 0);
    if (_isset(enable_pin1)) {
        digitalWrite(enable_pin1, LOW);
    }
    if (_isset(enable_pin2)) {
        digitalWrite(enable_pin2, LOW);
    }
}

int StepperDriver2PWM::init()
{
    pinMode(pwm1, OUTPUT);
    pinMode(pwm2, OUTPUT);
    pinMode(dir1a, OUTPUT);
    pinMode(dir2a, OUTPUT);
    if (_isset(dir1b)) {
        pinMode(dir1b, OUTPUT);
    }
    if (_isset(dir2b)) {
        pinMode(dir2b, OUTPUT);
    }
    if (_isset(enable_pin1)) {
        pinMode(enable_pin1, OUTPUT);
    }
    if (_isset(enable_pin2)) {
        pinMode(enable_pin2, OUTPUT);
    }

    if (!_isset(voltage_limit) || voltage_limit > voltage_power_supply) {
        voltage_limit = voltage_power_supply;
    }

    params = _configure2PWM(pwm_frequency, pwm1, pwm2);
    initialized = (params != SIMPLEFOC_DRIVER_INIT_FAILED);
    return initialized;
}

void StepperDriver2PWM::setPwm(float Ua, float Ub)
{
    Ua = _constrain(Ua, -voltage_limit, voltage_limit);
    Ub = _constrain(Ub, -voltage_limit, voltage_limit);

    float duty_cycle1 = _constrain(abs(Ua) / voltage_power_supply, 0.0f, 1.0f);
    float duty_cycle2 = _constrain(abs(Ub) / voltage_power_supply, 0.0f, 1.0f);

    digitalWrite(dir1a, Ua >= 0 ? LOW : HIGH);
    if (_isset(dir1b)) {
        digitalWrite(dir1b, Ua >= 0 ? HIGH : LOW);
    }
    digitalWrite(dir2a, Ub >= 0 ? LOW : HIGH);
    if (_isset(dir2b)) {
        digitalWrite(dir2b, Ub >= 0 ? HIGH : LOW);
    }

    _writeDutyCycle2PWM(duty_cycle1, duty_cycle2, params);
}

StepperDriver4PWM::StepperDriver4PWM(int ph1A, int ph1B, int ph2A, int ph2B, int en1, int en2)
{
    pwm1A = ph1A;
    pwm1B = ph1B;
    pwm2A = ph2A;
    pwm2B = ph2B;
    enable_pin1 = en1;
    enable_pin2 = en2;
    voltage_power_supply = DEF_POWER_SUPPLY;
    voltage_limit = NOT_SET;
    pwm_frequency = NOT_SET;
}

void StepperDriver4PWM::enable()
{
    if (_isset(enable_pin1)) {
        digitalWrite(enable_pin1, HIGH);
    }
    if (_isset(enable_pin2)) {
        digitalWrite(enable_pin2, HIGH);
    }
    setPwm(0, 0);
}

void StepperDriver4PWM::disable()
{
    setPwm(0, 0);
    if (_isset(enable_pin1)) {
        digitalWrite(enable_pin1, LOW);
    }
    if (_isset(enable_pin2)) {
        digitalWrite(enable_pin2, LOW);
    }
}

int StepperDriver4PWM::init()
{
    pinMode(pwm1A, OUTPUT);
    pinMode(pwm1B, OUTPUT);
    pinMode(pwm2A, OUTPUT);
    pinMode(pwm2B, OUTPUT);
    if (_isset(enable_pin1)) {
        pinMode(enable_pin1, OUTPUT);
    }
    if (_isset(enable_pin2)) {
        pinMode(enable_pin2, OUTPUT);
    }

    if (!_isset(voltage_limit) || voltage_limit > voltage_power_supply) {
        voltage_limit = voltage_power_supply;
    }

    params = _configure4PWM(pwm_frequency, pwm1A, pwm1B, pwm2A, pwm2B);
    initialized = (params != SIMPLEFOC_DRIVER_INIT_FAILED);
    return initialized;
}

void StepperDriver4PWM::setPwm(float Ualpha, float Ubeta)
{
    Ualpha = _constrain(Ualpha, -voltage_limit, voltage_limit);
    Ubeta = _constrain(Ubeta, -voltage_limit, voltage_limit);

    float duty_cycle1A = 0.0f;
    float duty_cycle1B = 0.0f;
    float duty_cycle2A = 0.0f;
    float duty_cycle2B = 0.0f;

    if (Ualpha > 0) {
        duty_cycle1B = _constrain(abs(Ualpha) / voltage_power_supply, 0.0f, 1.0f);
    } else {
        duty_cycle1A = _constrain(abs(Ualpha) / voltage_power_supply, 0.0f, 1.0f);
    }

    if (Ubeta > 0) {
        duty_cycle2B = _constrain(abs(Ubeta) / voltage_power_supply, 0.0f, 1.0f);
    } else {
        duty_cycle2A = _constrain(abs(Ubeta) / voltage_power_supply, 0.0f, 1.0f);
    }

    _writeDutyCycle4PWM(duty_cycle1A, duty_cycle1B, duty_cycle2A, duty_cycle2B, params);
}
