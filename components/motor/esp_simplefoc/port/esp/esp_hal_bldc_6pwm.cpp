/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_hal_bldc_6pwm.h"
#include "esp_log.h"
#include "esp_check.h"

#define _PWM_FREQUENCY 20000                           /*!< default frequency of MCPWM */
#define _PWM_FREQUENCY_MAX 50000                       /*!< Max frequency of MCPWM */
#define _PWM_TIMEBASE_RESOLUTION_HZ (10 * 1000 * 1000) /*!< Resolution of MCPWM */

static const char* TAG = "esp_hal_bldc_6pwm";

BLDCDriver6PWM::BLDCDriver6PWM(int phA_h, int phA_l, int phB_h, int phB_l, int phC_h, int phC_l, int en, int mcpwm_group_id)
{
    pwmA_h = phA_h;
    pwmB_h = phB_h;
    pwmC_h = phC_h;
    pwmA_l = phA_l;
    pwmB_l = phB_l;
    pwmC_l = phC_l;

    // enable_pin pin
    enable_pin = en;

    // default power-supply value
    voltage_power_supply = DEF_POWER_SUPPLY;
    voltage_limit = NOT_SET;
    pwm_frequency = NOT_SET;

    // dead zone initial - 2%
    dead_zone = 0.02f;

    this->mcpwm_group_id = mcpwm_group_id;
}

void BLDCDriver6PWM::enable()
{
    // enable_pin the driver - if enable_pin pin available
    if (_isset(enable_pin)) {
        digitalWrite(enable_pin, enable_active_high);
    }
    // set phase state enabled
    setPhaseState(PhaseState::PHASE_ON, PhaseState::PHASE_ON, PhaseState::PHASE_ON);
    // set zero to PWM
    setPwm(0, 0, 0);
}

void BLDCDriver6PWM::disable()
{
    // set phase state to disabled
    setPhaseState(PhaseState::PHASE_OFF, PhaseState::PHASE_OFF, PhaseState::PHASE_OFF);
    // set zero to PWM
    setPwm(0, 0, 0);
    // disable the driver - if enable_pin pin available
    if (_isset(enable_pin)) {
        digitalWrite(enable_pin, !enable_active_high);
    }
}

int BLDCDriver6PWM::init()
{
    // PWM pins
    pinMode(pwmA_h, OUTPUT);
    pinMode(pwmB_h, OUTPUT);
    pinMode(pwmC_h, OUTPUT);
    pinMode(pwmA_l, OUTPUT);
    pinMode(pwmB_l, OUTPUT);
    pinMode(pwmC_l, OUTPUT);
    if (_isset(enable_pin)) {
        pinMode(enable_pin, OUTPUT);
    }

    // sanity check for the voltage limit configuration
    if (!_isset(voltage_limit) || voltage_limit > voltage_power_supply) {
        voltage_limit = voltage_power_supply;
    }

    // set phase state to disabled
    phase_state[0] = PhaseState::PHASE_OFF;
    phase_state[1] = PhaseState::PHASE_OFF;
    phase_state[2] = PhaseState::PHASE_OFF;

    // set zero to PWM
    dc_a = dc_b = dc_c = 0;

    // configure 6pwm
    // hardware specific function - depending on driver and mcu
    if (mcpwm_group_id < 0 || mcpwm_group_id >= SOC_MCPWM_GROUPS) {
        ESP_LOGE(TAG, "MCPWM group id: %d is not available\n", mcpwm_group_id);
        return 0;
    }

    // Init mcpwm driver
    mcpwm_timer_config_t timer_config = {
        .group_id = mcpwm_group_id,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = _PWM_TIMEBASE_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
        .period_ticks = (uint32_t)(1 * _PWM_TIMEBASE_RESOLUTION_HZ / _PWM_FREQUENCY),
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_operator_config_t operator_config = {
        .group_id = mcpwm_group_id,
    };

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper[i], timer));
    }

    mcpwm_comparator_config_t comparator_config = {0};
    comparator_config.flags.update_cmp_on_tez = true;
    comparator_config.flags.update_cmp_on_tep = false;
    comparator_config.flags.update_cmp_on_sync = false;

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_new_comparator(oper[i], &comparator_config, &comparator[i]));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[i], (0)));
    }

    mcpwm_generator_config_t generator_config = {};
    int gen_gpios[3][2] = {
        {pwmA_h, pwmA_l},
        {pwmB_h, pwmB_l},
        {pwmC_h, pwmC_l},
    };

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            generator_config.gen_gpio_num = gen_gpios[i][j];
            ESP_ERROR_CHECK(mcpwm_new_generator(oper[i], &generator_config, &generator[i][j]));
        }
    }

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator[i][0],
                                                                     MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_LOW),
                                                                     MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, comparator[i], MCPWM_GEN_ACTION_HIGH),
                                                                     MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
    }

    mcpwm_dead_time_config_t dt_config = {
        .posedge_delay_ticks = (uint32_t)(dead_zone * timer_config.period_ticks),
    };

    mcpwm_dead_time_config_t inv_dt_config = {
        .negedge_delay_ticks = (uint32_t)(dead_zone * timer_config.period_ticks),
    };
    inv_dt_config.flags.invert_output = true;

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator[i][0], generator[i][0], &dt_config));
        ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(generator[i][0], generator[i][1], &inv_dt_config));
    }

    // enable mcpwm
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // save timer period
    mcpwm_period = timer_config.period_ticks / 2;
    initialized = 1;

    return 1;
}

int BLDCDriver6PWM::deinit()
{
    if (initialized) {
        ESP_ERROR_CHECK(mcpwm_timer_disable(timer));

        for (int i = 0; i < 3; i++) {
            ESP_ERROR_CHECK(mcpwm_del_generator(generator[i][0]));
            ESP_ERROR_CHECK(mcpwm_del_generator(generator[i][1]));
            ESP_ERROR_CHECK(mcpwm_del_comparator(comparator[i]));
            ESP_ERROR_CHECK(mcpwm_del_operator(oper[i]));
        }
        ESP_ERROR_CHECK(mcpwm_del_timer(timer));
        initialized = 0;
        return 1;
    }

    return 0;
}

void BLDCDriver6PWM::setPwm(float Ua, float Ub, float Uc)
{
    // limit the voltage in driver
    Ua = _constrain(Ua, 0, voltage_limit);
    Ub = _constrain(Ub, 0, voltage_limit);
    Uc = _constrain(Uc, 0, voltage_limit);
    // calculate duty cycle
    // limited in [0,1]
    dc_a = _constrain(Ua / voltage_power_supply, 0.0f, 1.0f);
    dc_b = _constrain(Ub / voltage_power_supply, 0.0f, 1.0f);
    dc_c = _constrain(Uc / voltage_power_supply, 0.0f, 1.0f);
    // hardware specific writing
    // hardware specific function - depending on driver and mcu
    halPwmWrite();
}

void BLDCDriver6PWM::setPhaseState(PhaseState sa, PhaseState sb, PhaseState sc)
{
    phase_state[0] = sa;
    phase_state[1] = sb;
    phase_state[2] = sc;
}

void BLDCDriver6PWM::halPwmWrite()
{
    if (!initialized) {
        return;
    }

    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[0], (uint32_t)((mcpwm_period * dc_a))));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[1], (uint32_t)((mcpwm_period * dc_b))));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[2], (uint32_t)((mcpwm_period * dc_c))));
}
