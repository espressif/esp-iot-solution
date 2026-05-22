/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_hal_bldc_6pwm.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_check.h"
#include "soc/soc_caps.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#include "hal/mcpwm_periph.h"
#endif

#define _PWM_FREQUENCY 20000                           /*!< default frequency of MCPWM */
#define _PWM_FREQUENCY_MAX 50000                       /*!< Max frequency of MCPWM */
#define _PWM_TIMEBASE_RESOLUTION_HZ (10 * 1000 * 1000) /*!< Resolution of MCPWM */

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
#define ESP_SIMPLEFOC_MCPWM_GROUPS SOC_MCPWM_GROUPS
#else
#define ESP_SIMPLEFOC_MCPWM_GROUPS (sizeof(soc_mcpwm_signals) / sizeof(soc_mcpwm_signals[0]))
#endif

static const char* TAG = "esp_hal_bldc_6pwm";

extern bool IRAM_ATTR esp_simplefoc_lowside_on_mcpwm_full(mcpwm_timer_handle_t timer,
                                                          const mcpwm_timer_event_data_t *edata,
                                                          void *user_data);

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

    for (int i = 0; i < 3; i++) {
        oper[i] = NULL;
        comparator[i] = NULL;
        generator[i][0] = NULL;
        generator[i][1] = NULL;
    }
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
    EspMcpwmDriverParams *driver_params = new EspMcpwmDriverParams;
    esp_err_t ret = ESP_OK;

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
    {
        ESP_GOTO_ON_FALSE(mcpwm_group_id >= 0 && mcpwm_group_id < (int)ESP_SIMPLEFOC_MCPWM_GROUPS,
                          ESP_ERR_INVALID_ARG, init_fail, TAG, "MCPWM group id: %d is not available", mcpwm_group_id);

        // Init mcpwm driver
        mcpwm_timer_config_t timer_config = {
            .group_id = mcpwm_group_id,
            .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
            .resolution_hz = _PWM_TIMEBASE_RESOLUTION_HZ,
            .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
            .period_ticks = (uint32_t)(1 * _PWM_TIMEBASE_RESOLUTION_HZ / _PWM_FREQUENCY),
        };
        ESP_GOTO_ON_ERROR(mcpwm_new_timer(&timer_config, &timer), init_fail, TAG, "failed to create MCPWM timer");

        mcpwm_operator_config_t operator_config = {
            .group_id = mcpwm_group_id,
        };

        for (int i = 0; i < 3; i++) {
            ESP_GOTO_ON_ERROR(mcpwm_new_operator(&operator_config, &oper[i]),
                              init_fail, TAG, "failed to create MCPWM operator");
            ESP_GOTO_ON_ERROR(mcpwm_operator_connect_timer(oper[i], timer),
                              init_fail, TAG, "failed to connect MCPWM operator");
        }

        mcpwm_comparator_config_t comparator_config = {0};
        comparator_config.flags.update_cmp_on_tez = true;
        comparator_config.flags.update_cmp_on_tep = false;
        comparator_config.flags.update_cmp_on_sync = false;

        for (int i = 0; i < 3; i++) {
            ESP_GOTO_ON_ERROR(mcpwm_new_comparator(oper[i], &comparator_config, &comparator[i]),
                              init_fail, TAG, "failed to create comparator");
            ESP_GOTO_ON_ERROR(mcpwm_comparator_set_compare_value(comparator[i], (0)),
                              init_fail, TAG, "failed to clear comparator");
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
                ESP_GOTO_ON_ERROR(mcpwm_new_generator(oper[i], &generator_config, &generator[i][j]),
                                  init_fail, TAG, "failed to create generator");
            }
        }

        for (int i = 0; i < 3; i++) {
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
            ESP_GOTO_ON_ERROR(mcpwm_generator_set_actions_on_compare_event(generator[i][0],
                                                                           MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_LOW),
                                                                           MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, comparator[i], MCPWM_GEN_ACTION_HIGH),
                                                                           MCPWM_GEN_COMPARE_EVENT_ACTION_END()),
                              init_fail, TAG, "failed to set compare actions");
#else
            ESP_GOTO_ON_ERROR(mcpwm_generator_set_action_on_compare_event(generator[i][0],
                                                                          MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_LOW)),
                              init_fail, TAG, "failed to set compare action");
            ESP_GOTO_ON_ERROR(mcpwm_generator_set_action_on_compare_event(generator[i][0],
                                                                          MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, comparator[i], MCPWM_GEN_ACTION_HIGH)),
                              init_fail, TAG, "failed to set compare action");
#endif
        }

        mcpwm_dead_time_config_t dt_config = {
            .posedge_delay_ticks = (uint32_t)(dead_zone * timer_config.period_ticks),
        };

        mcpwm_dead_time_config_t inv_dt_config = {
            .negedge_delay_ticks = (uint32_t)(dead_zone * timer_config.period_ticks),
        };
        inv_dt_config.flags.invert_output = true;

        for (int i = 0; i < 3; i++) {
            ESP_GOTO_ON_ERROR(mcpwm_generator_set_dead_time(generator[i][0], generator[i][0], &dt_config),
                              init_fail, TAG, "failed to set dead time");
            ESP_GOTO_ON_ERROR(mcpwm_generator_set_dead_time(generator[i][0], generator[i][1], &inv_dt_config),
                              init_fail, TAG, "failed to set dead time");
        }

        driver_params->group_id = mcpwm_group_id;
        driver_params->timer = timer;
        driver_params->period_ticks = timer_config.period_ticks;
        driver_params->center_aligned = true;
        for (int i = 0; i < 3; i++) {
            driver_params->oper[i] = oper[i];
            driver_params->comparator[i] = comparator[i];
            driver_params->generator[i][0] = generator[i][0];
            driver_params->generator[i][1] = generator[i][1];
        }

        mcpwm_timer_event_callbacks_t timer_cbs = {
            .on_full = esp_simplefoc_lowside_on_mcpwm_full,
        };
        ESP_GOTO_ON_ERROR(mcpwm_timer_register_event_callbacks(timer, &timer_cbs, driver_params),
                          init_fail, TAG, "failed to register timer callback");

        // enable mcpwm
        ESP_GOTO_ON_ERROR(mcpwm_timer_enable(timer), init_fail, TAG, "failed to enable MCPWM timer");
        ESP_GOTO_ON_ERROR(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP), init_fail, TAG, "failed to start MCPWM timer");

        // save timer period
        mcpwm_period = timer_config.period_ticks / 2;
        params = driver_params;
        initialized = 1;
    }

    return 1;

init_fail:
    (void)ret;
    if (timer != NULL) {
        mcpwm_timer_disable(timer);
    }
    for (int i = 0; i < 3; i++) {
        if (generator[i][0] != NULL) {
            mcpwm_del_generator(generator[i][0]);
            generator[i][0] = NULL;
        }
        if (generator[i][1] != NULL) {
            mcpwm_del_generator(generator[i][1]);
            generator[i][1] = NULL;
        }
        if (comparator[i] != NULL) {
            mcpwm_del_comparator(comparator[i]);
            comparator[i] = NULL;
        }
        if (oper[i] != NULL) {
            mcpwm_del_operator(oper[i]);
            oper[i] = NULL;
        }
    }
    if (timer != NULL) {
        mcpwm_del_timer(timer);
        timer = NULL;
    }
    delete driver_params;
    return 0;
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
        delete static_cast<EspMcpwmDriverParams *>(params);
        params = nullptr;
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
