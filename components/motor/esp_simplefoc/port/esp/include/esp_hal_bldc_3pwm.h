/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vector>
#include "BLDCDriver.h"
#include "foc_utils.h"
#include "time_utils.h"
#include "defaults.h"
#include "drivers/hardware_api.h"
#include "driver/mcpwm_prelude.h"
#include "driver/ledc.h"

#define _PWM_FREQUENCY 20000                           /*!< default frequency of MCPWM */
#define _PWM_FREQUENCY_MAX 50000                       /*!< Max frequency of MCPWM */
#define _PWM_TIMEBASE_RESOLUTION_HZ (10 * 1000 * 1000) /*!< Resolution of MCPWM */
#define _LEDC_TIMER LEDC_TIMER_0                       /*!< Timer od LEDC */
#define _LEDC_MODE LEDC_LOW_SPEED_MODE                 /*!< Speed Mode of LEDC */
#define _LEDC_DUTY_RES LEDC_TIMER_9_BIT                /*!< Resolution of LEDC */
#define _LEDC_DUTY (511)                               /*!< Max duty of LEDC */
#define _LEDC_FREQUENCY (20 * 1000)                    /*!< Frequency of LEDC */

enum class DriverMode {
    mcpwm = 0, /*!< Foc hardware drive mode：MCPWM */
    ledc = 1,  /*!< Foc hardware drive mode：LEDC */
};

class BLDCDriver3PWM : public BLDCDriver {
public:
    /**
     * @brief Construct a new BLDCDriver3PWM object
     *
     * @param phA A phase pwm pin
     * @param phB B phase pwm pin
     * @param phC C phase pwm pin
     * @param en1 enable pin (optional input)
     * @param en2 enable pin (optional input)
     * @param en3 enable pin (optional input)
     */
    BLDCDriver3PWM(int phA, int phB, int phC, int en1 = NOT_SET, int en2 = NOT_SET, int en3 = NOT_SET);

    /**
     * @brief Motor hardware init function.
     *
     * @return
     *     - 0 Failed
     *     - 1 Success
     */
    int init() override;

    /**
     * @brief Motor hardware init function, using MCPWM.
     *
     * @param _mcpwm_group mcpwm group, such as 0
     * @return
     *     - 0 Failed
     *     - 1 Success
     */
    int init(int _mcpwm_group);

    /**
     * @brief Motor hardware init function, using LEDC.
     *
     * @param _ledc_channels ledc channel, such as {1,2,3}
     * @return
     *     - 0 Failed
     *     - 1 Success
     */
    int init(std::vector<int> _ledc_channels);

    /**
     * @brief Motor hardware deinit function.
     *
     * @return
     *     - 0 Failed
     *     - 1 Success
     */
    int deinit();

    /**
     * @brief Motor disable function.
     *
     */
    void disable() override;

    /**
     * @brief Motor enable function.
     *
     */
    void enable() override;

    // hardware variables
    int pwmA;                       /*!< phase A pwm pin number */
    int pwmB;                       /*!< phase B pwm pin number */
    int pwmC;                       /*!< phase C pwm pin number */
    int enableA_pin;                /*!< enable pin number */
    int enableB_pin;                /*!< enable pin number */
    int enableC_pin;                /*!< enable pin number */
    bool enable_active_high = true; /*!< enable driver level */

    /**
     * Set phase voltages to the harware.
     *
     * @param Ua - phase A voltage
     * @param Ub - phase B voltage
     * @param Uc - phase C voltage
     */
    void setPwm(float Ua, float Ub, float Uc) override;

    /**
     * @brief Set phase voltages to the harware.
     *
     * @param sa phase A state : active / disabled ( high impedance )
     * @param sb phase B state : active / disabled ( high impedance )
     * @param sc phase C state : active / disabled ( high impedance )
     */
    virtual void setPhaseState(PhaseState sa, PhaseState sb, PhaseState sc) override;

    /**
     * @brief PWM generating function.
     *
     */
    void halPwmWrite();

private:
    DriverMode driverMode;
    int mcpwm_group;
    std::vector<int> ledc_channels;
    mcpwm_gen_handle_t generator[3] = {};
    mcpwm_cmpr_handle_t comparator[3];
    mcpwm_oper_handle_t oper[3];
    mcpwm_timer_handle_t timer = NULL;
    uint32_t mcpwm_period;
};
