/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_hal_mcpwm.h"
#include "esp_log.h"
#include "drivers/hardware_api.h"

int group_id = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
// Using ESP_IDF 4.4.* MCPWM driver.

bldc_3pwm_motor_slots_t esp32_bldc_3pwm_motor_slots[4] = {
    {_EMPTY_SLOT, &MCPWM0, MCPWM_UNIT_0, MCPWM_OPR_A, MCPWM0A, MCPWM1A, MCPWM2A}, // 1st motor will be MCPWM0 channel A
    {_EMPTY_SLOT, &MCPWM0, MCPWM_UNIT_0, MCPWM_OPR_B, MCPWM0B, MCPWM1B, MCPWM2B}, // 2nd motor will be MCPWM0 channel B
    {_EMPTY_SLOT, &MCPWM1, MCPWM_UNIT_1, MCPWM_OPR_A, MCPWM0A, MCPWM1A, MCPWM2A}, // 3rd motor will be MCPWM1 channel A
    {_EMPTY_SLOT, &MCPWM1, MCPWM_UNIT_1, MCPWM_OPR_B, MCPWM0B, MCPWM1B, MCPWM2B}  // 4th motor will be MCPWM1 channel B
};

// define stepper motor slots array
bldc_6pwm_motor_slots_t esp32_bldc_6pwm_motor_slots[2] = {
    {_EMPTY_SLOT, &MCPWM0, MCPWM_UNIT_0, MCPWM_OPR_A, MCPWM_OPR_B, MCPWM0A, MCPWM1A, MCPWM2A, MCPWM0B, MCPWM1B, MCPWM2B}, // 1st motor will be on MCPWM0
    {_EMPTY_SLOT, &MCPWM1, MCPWM_UNIT_1, MCPWM_OPR_A, MCPWM_OPR_B, MCPWM0A, MCPWM1A, MCPWM2A, MCPWM0B, MCPWM1B, MCPWM2B}, // 1st motor will be on MCPWM0
};

// define 4pwm stepper motor slots array
stepper_4pwm_motor_slots_t esp32_stepper_4pwm_motor_slots[2] = {
    {_EMPTY_SLOT, &MCPWM0, MCPWM_UNIT_0, MCPWM_OPR_A, MCPWM_OPR_B, MCPWM0A, MCPWM1A, MCPWM0B, MCPWM1B}, // 1st motor will be on MCPWM0
    {_EMPTY_SLOT, &MCPWM1, MCPWM_UNIT_1, MCPWM_OPR_A, MCPWM_OPR_B, MCPWM0A, MCPWM1A, MCPWM0B, MCPWM1B}, // 1st motor will be on MCPWM1
};

// define 2pwm stepper motor slots array
stepper_2pwm_motor_slots_t esp32_stepper_2pwm_motor_slots[4] = {
    {_EMPTY_SLOT, &MCPWM0, MCPWM_UNIT_0, MCPWM_OPR_A, MCPWM0A, MCPWM1A}, // 1st motor will be MCPWM0 channel A
    {_EMPTY_SLOT, &MCPWM0, MCPWM_UNIT_0, MCPWM_OPR_B, MCPWM0B, MCPWM1B}, // 2nd motor will be MCPWM0 channel B
    {_EMPTY_SLOT, &MCPWM1, MCPWM_UNIT_1, MCPWM_OPR_A, MCPWM0A, MCPWM1A}, // 3rd motor will be MCPWM1 channel A
    {_EMPTY_SLOT, &MCPWM1, MCPWM_UNIT_1, MCPWM_OPR_B, MCPWM0B, MCPWM1B}  // 4th motor will be MCPWM1 channel B
};

/**
 * @description: Configuring high frequency pwm timer.
 * @param {long} pwm_frequency
 * @param {mcpwm_dev_t} *mcpwm_num
 * @param {mcpwm_unit_t} mcpwm_unit
 * @param {float} dead_zone
 * @return {*}
 */
void _configureTimerFrequency(long pwm_frequency, mcpwm_dev_t *mcpwm_num, mcpwm_unit_t mcpwm_unit, float dead_zone = NOT_SET)
{

    mcpwm_config_t pwm_config;
    pwm_config.counter_mode = MCPWM_UP_DOWN_COUNTER;    // Up-down counter (triangle wave)
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;           // Active HIGH
    pwm_config.frequency = 2 * pwm_frequency;           // set the desired freq - just a placeholder for now https://github.com/simplefoc/Arduino-FOC/issues/76
    mcpwm_init(mcpwm_unit, MCPWM_TIMER_0, &pwm_config); // Configure PWM0A & PWM0B with above settings
    mcpwm_init(mcpwm_unit, MCPWM_TIMER_1, &pwm_config); // Configure PWM1A & PWM1B with above settings
    mcpwm_init(mcpwm_unit, MCPWM_TIMER_2, &pwm_config); // Configure PWM2A & PWM2B with above settings

    if (_isset(dead_zone))
    {
        // dead zone is configured
        float dead_time = (float)(_MCPWM_FREQ / (pwm_frequency)) * dead_zone;
        mcpwm_deadtime_enable(mcpwm_unit, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, dead_time / 2.0, dead_time / 2.0);
        mcpwm_deadtime_enable(mcpwm_unit, MCPWM_TIMER_1, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, dead_time / 2.0, dead_time / 2.0);
        mcpwm_deadtime_enable(mcpwm_unit, MCPWM_TIMER_2, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, dead_time / 2.0, dead_time / 2.0);
    }
    _delay(100);

    mcpwm_stop(mcpwm_unit, MCPWM_TIMER_0);
    mcpwm_stop(mcpwm_unit, MCPWM_TIMER_1);
    mcpwm_stop(mcpwm_unit, MCPWM_TIMER_2);

    // manual configuration due to the lack of config flexibility in mcpwm_init()
    mcpwm_num->clk_cfg.clk_prescale = 0;
    // calculate prescaler and period
    // step 1: calculate the prescaler using the default pwm resolution
    // prescaler = bus_freq / (pwm_freq * default_pwm_res) - 1
    int prescaler = ceil((double)_MCPWM_FREQ / (double)_PWM_RES_DEF / 2.0f / (double)pwm_frequency) - 1;
    // constrain prescaler
    prescaler = _constrain(prescaler, 0, 128);
    // now calculate the real resolution timer period necessary (pwm resolution)
    // pwm_res = bus_freq / (pwm_freq * (prescaler + 1))
    int resolution_corrected = (double)_MCPWM_FREQ / 2.0f / (double)pwm_frequency / (double)(prescaler + 1);
    // if pwm resolution too low lower the prescaler
    if (resolution_corrected < _PWM_RES_MIN && prescaler > 0)
        resolution_corrected = (double)_MCPWM_FREQ / 2.0f / (double)pwm_frequency / (double)(--prescaler + 1);
    resolution_corrected = _constrain(resolution_corrected, _PWM_RES_MIN, _PWM_RES_MAX);

    // set prescaler
    mcpwm_num->timer[0].timer_cfg0.timer_prescale = prescaler;
    mcpwm_num->timer[1].timer_cfg0.timer_prescale = prescaler;
    mcpwm_num->timer[2].timer_cfg0.timer_prescale = prescaler;
    _delay(1);
    // set period
    mcpwm_num->timer[0].timer_cfg0.timer_period = resolution_corrected;
    mcpwm_num->timer[1].timer_cfg0.timer_period = resolution_corrected;
    mcpwm_num->timer[2].timer_cfg0.timer_period = resolution_corrected;
    _delay(1);
    mcpwm_num->timer[0].timer_cfg0.timer_period_upmethod = 0;
    mcpwm_num->timer[1].timer_cfg0.timer_period_upmethod = 0;
    mcpwm_num->timer[2].timer_cfg0.timer_period_upmethod = 0;
    _delay(1);
    // _delay(1);
    // restart the timers
    mcpwm_start(mcpwm_unit, MCPWM_TIMER_0);
    mcpwm_start(mcpwm_unit, MCPWM_TIMER_1);
    mcpwm_start(mcpwm_unit, MCPWM_TIMER_2);
    _delay(1);

    mcpwm_sync_config_t sync_conf = {
        .sync_sig = MCPWM_SELECT_TIMER0_SYNC,
        .timer_val = 0,
        .count_direction = MCPWM_TIMER_DIRECTION_UP};
    mcpwm_sync_configure(mcpwm_unit, MCPWM_TIMER_0, &sync_conf);
    mcpwm_sync_configure(mcpwm_unit, MCPWM_TIMER_1, &sync_conf);
    mcpwm_sync_configure(mcpwm_unit, MCPWM_TIMER_2, &sync_conf);

    // Enable sync event for all timers to be the TEZ of timer0
    mcpwm_set_timer_sync_output(mcpwm_unit, MCPWM_TIMER_0, MCPWM_SWSYNC_SOURCE_TEZ);
}

/**
 * @description: Setting the high pwm frequency to the supplied pins.
 * @param {long} pwm_frequency
 * @param {int} pinA
 * @param {int} pinB
 * @param {int} pinC
 * @return {*}
 */
void *_configure3PWM(long pwm_frequency, const int pinA, const int pinB, const int pinC)
{
    if (!pwm_frequency || !_isset(pwm_frequency))
        pwm_frequency = _PWM_FREQUENCY; // default frequency 25hz
    else
        pwm_frequency = _constrain(pwm_frequency, 0, _PWM_FREQUENCY_MAX); // constrain to 40kHz max

    bldc_3pwm_motor_slots_t m_slot = {};

    // determine which motor are we connecting
    // and set the appropriate configuration parameters
    int slot_num;
    for (slot_num = 0; slot_num < 4; slot_num++)
    {
        if (esp32_bldc_3pwm_motor_slots[slot_num].pinA == _EMPTY_SLOT)
        { // put the new motor in the first empty slot
            esp32_bldc_3pwm_motor_slots[slot_num].pinA = pinA;
            m_slot = esp32_bldc_3pwm_motor_slots[slot_num];
            break;
        }
    }
    // disable all the slots with the same MCPWM
    // disable 2pwm steppr motor which would go in the same slot
    esp32_stepper_2pwm_motor_slots[slot_num].pin1pwm = _TAKEN_SLOT;
    if (slot_num < 2)
    {
        // slot 0 of the 4pwm stepper
        esp32_stepper_4pwm_motor_slots[0].pin1A = _TAKEN_SLOT;
        // slot 0 of the 6pwm bldc
        esp32_bldc_6pwm_motor_slots[0].pinAH = _TAKEN_SLOT;
    }
    else
    {
        // slot 1 of the stepper
        esp32_stepper_4pwm_motor_slots[1].pin1A = _TAKEN_SLOT;
        // slot 1 of the 6pwm bldc
        esp32_bldc_6pwm_motor_slots[1].pinAH = _TAKEN_SLOT;
    }
    // configure pins
    mcpwm_gpio_init(m_slot.mcpwm_unit, m_slot.mcpwm_a, pinA);
    mcpwm_gpio_init(m_slot.mcpwm_unit, m_slot.mcpwm_b, pinB);
    mcpwm_gpio_init(m_slot.mcpwm_unit, m_slot.mcpwm_c, pinC);

    // configure the timer
    _configureTimerFrequency(pwm_frequency, m_slot.mcpwm_num, m_slot.mcpwm_unit);

    ESP32MCPWMDriverParams *params = new ESP32MCPWMDriverParams{
        .pwm_frequency = pwm_frequency,
        .mcpwm_dev = m_slot.mcpwm_num,
        .mcpwm_unit = m_slot.mcpwm_unit,
        .mcpwm_operator1 = m_slot.mcpwm_operator};
    return params;
}

/**
 * @description: Write 3PWM Duty.
 * @param {float} dc_a
 * @param {float} dc_b
 * @param {float} dc_c
 * @param {void} *params
 * @return {*}
 */
void _writeDutyCycle3PWM(float dc_a, float dc_b, float dc_c, void *params)
{
    // se the PWM on the slot timers
    // transform duty cycle from [0,1] to [0,100]
    mcpwm_set_duty(((ESP32MCPWMDriverParams *)params)->mcpwm_unit, MCPWM_TIMER_0, ((ESP32MCPWMDriverParams *)params)->mcpwm_operator1, dc_a * 100.0);
    mcpwm_set_duty(((ESP32MCPWMDriverParams *)params)->mcpwm_unit, MCPWM_TIMER_1, ((ESP32MCPWMDriverParams *)params)->mcpwm_operator1, dc_b * 100.0);
    mcpwm_set_duty(((ESP32MCPWMDriverParams *)params)->mcpwm_unit, MCPWM_TIMER_2, ((ESP32MCPWMDriverParams *)params)->mcpwm_operator1, dc_c * 100.0);
}

#else
// Using ESP_IDF 5.* MCPWM driver.

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_9_BIT // Set duty resolution to 9 bits
#define LEDC_DUTY (511)                // Duty Max = 2^9 = 511
#define LEDC_FREQUENCY (20 * 1000)      // Frequency in Hertz. Set frequency at 30 kHz
ledc_channel_t ledc_timer_channels[2][3] = {{LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2}, {LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5}};

/**
 * @description: Configure three pwm channels.
 * @param {long} pwm_frequency
 * @param {int} pinA
 * @param {int} pinB
 * @param {int} pinC
 * @return {*}
 */
void *_configure3PWM(long pwm_frequency, const int pinA, const int pinB, const int pinC)
{
    if (!pwm_frequency || !_isset(pwm_frequency))
    {
        pwm_frequency = _PWM_FREQUENCY; // default frequency 25KHZ
    }
    else
    {
        pwm_frequency = _constrain(pwm_frequency, 0, _PWM_FREQUENCY_MAX); // constrain to 50kHZ max
    }

    if (group_id < 2)
    {
        // using mcpwm driver.
        mcpwm_timer_handle_t timer = NULL;
        mcpwm_timer_config_t timer_config = {
            .group_id = group_id,
            .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
            .resolution_hz = _PWM_TIMEBASE_RESOLUTION_HZ,
            .count_mode = MCPWM_TIMER_COUNT_MODE_UP,                                     // centeral mode
            .period_ticks = (uint32_t)(1 * _PWM_TIMEBASE_RESOLUTION_HZ / pwm_frequency), // real frequency is pwm_frequency/2
        };
        ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

        mcpwm_oper_handle_t oper[3];
        mcpwm_operator_config_t operator_config = {
            .group_id = group_id, // operator must be in the same group to the timer
        };

        for (int i = 0; i < 3; i++)
        {
            ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper[i]));
        }

        for (int i = 0; i < 3; i++)
        {
            ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper[i], timer));
        }

        mcpwm_cmpr_handle_t comparator[3];
        mcpwm_comparator_config_t comparator_config;
        comparator_config.flags.update_cmp_on_tez = true;
        comparator_config.flags.update_cmp_on_tep = false;
        comparator_config.flags.update_cmp_on_sync = false;

        for (int i = 0; i < 3; i++)
        {
            ESP_ERROR_CHECK(mcpwm_new_comparator(oper[i], &comparator_config, &comparator[i]));
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[i], (0)));
        }

        // bind gpio port
        mcpwm_gen_handle_t generator[3] = {};
        mcpwm_generator_config_t generator_config = {};
        const int gen_gpios[3] = {pinA, pinB, pinC}; // three gpio port
        for (int i = 0; i < 3; i++)
        {
            generator_config.gen_gpio_num = gen_gpios[i];
            ESP_ERROR_CHECK(mcpwm_new_generator(oper[i], &generator_config, &generator[i]));
        }

        for (int i = 0; i < 3; i++)
        {
            ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator[i],
                                                                      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
            // go low on compare threshold
            ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator[i],
                                                                        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_LOW)));
        }

        // enable mcpwm
        ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

        // copy params
        ESP32MCPWMDriverParams *params = new ESP32MCPWMDriverParams{};
        params->group_id = group_id;
        params->pwm_frequency = pwm_frequency;

        // use mcpwm driver.
        params->timer = timer;
        params->is_config = 1;

        for (int i = 0; i < 3; i++)
        {
            params->comparator[i] = comparator[i];
        }
        params->pwm_timeperiod = (uint32_t)(timer_config.period_ticks);
        printf("Success. Using MCPWMDriver, id:%d\n", group_id);
        group_id++;

        return params;
    }
    else if (group_id >= 2 && group_id < 4)
    {
        // use ledc driver.
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_MODE,
            .duty_resolution = LEDC_DUTY_RES,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = LEDC_FREQUENCY, // Set output frequency at 20 kHz
            .clk_cfg = LEDC_USE_APB_CLK};
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        const int pins[3] = {pinA, pinB, pinC}; // save pins

        ledc_channel_config_t ledc_channel;
        for (int i = 0; i < 3; i++)
        {
            ledc_channel.speed_mode = LEDC_MODE;
            ledc_channel.timer_sel = LEDC_TIMER_0;
            ledc_channel.duty = 0;
            ledc_channel.hpoint = 0;
            ledc_channel.intr_type = LEDC_INTR_DISABLE;
            ledc_channel.channel = ledc_timer_channels[group_id - 2][i];
            ledc_channel.gpio_num = pins[i];
            ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
        }

        // copy params
        ESP32MCPWMDriverParams *params = new ESP32MCPWMDriverParams{};
        params->group_id = group_id;
        params->pwm_frequency = pwm_frequency;
        params->pwm_timeperiod = LEDC_DUTY; // 11bits

        for (int i = 0; i < 3; i++)
        {
            params->ledc_channel[i] = ledc_timer_channels[group_id - 2][i];
        }
        printf("Success. Using LedcDriver, id:%d\n", group_id);

        group_id++;

        return params;
    }

    printf("esp_simplefoc only supports four motors\n");

    return NULL;
}

/**
 * @description: Write PWM Duty.
 * @param {float} dc_a
 * @param {float} dc_b
 * @param {float} dc_c
 * @param {void} *params
 * @return {*}
 */
void _writeDutyCycle3PWM(float dc_a, float dc_b, float dc_c, void *params)
{
    ESP32MCPWMDriverParams *driverParams = ((ESP32MCPWMDriverParams *)params);
    if (driverParams->group_id < 2)
    {
        // use mcpwm driver
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(driverParams->comparator[0], (uint32_t)((uint32_t)(driverParams->pwm_timeperiod * dc_a))));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(driverParams->comparator[1], (uint32_t)((uint32_t)(driverParams->pwm_timeperiod * dc_b))));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(driverParams->comparator[2], (uint32_t)((uint32_t)(driverParams->pwm_timeperiod * dc_c))));
    }
    else if (driverParams->group_id >= 2 && driverParams->group_id < 4)
    {
        // use ledc driver
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, driverParams->ledc_channel[0], (uint32_t)((uint32_t)(driverParams->pwm_timeperiod * dc_a))));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, driverParams->ledc_channel[0]));
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, driverParams->ledc_channel[1], (uint32_t)((uint32_t)(driverParams->pwm_timeperiod * dc_b))));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, driverParams->ledc_channel[1]));
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, driverParams->ledc_channel[2], (uint32_t)((uint32_t)(driverParams->pwm_timeperiod * dc_c))));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, driverParams->ledc_channel[2]));
    }
}

#endif
