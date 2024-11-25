/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "BLDCDriver.h"
#include "foc_utils.h"
#include "time_utils.h"
#include "defaults.h"
#include "drivers/hardware_api.h"
#include "driver/mcpwm_prelude.h"

class BLDCDriver6PWM : public BLDCDriver {
public:
    /**
     * @brief Construct a new BLDCDriver6PWM object
     *
     * @param phA_h High-side PWM pin for phase A
     * @param phA_l Low-side PWM pin for phase A
     * @param phB_h High-side PWM pin for phase B
     * @param phB_l Low-side PWM pin for phase B
     * @param phC_h High-side PWM pin for phase C
     * @param phC_l Low-side PWM pin for phase C
     * @param en Optional enable pin (default: NOT_SET)
     * @param mcpwm_group_id The MCPWM group ID to be used (default: 0)
     */
    BLDCDriver6PWM(int phA_h, int phA_l, int phB_h, int phB_l, int phC_h, int phC_l, int en = NOT_SET, int mcpwm_group_id = 0);

    /**
     * @brief Motor hardware init function
     *
     * @return
     *     - 0 Failed
     *     - 1 Success
     */
    int init() override;

    /**
     * @brief Motor hardware deinit function
     *
     * @return
     *     - 0 Failed
     *     - 1 Success
     */
    int deinit();

    /**
     * @brief Motor disable function
     *
     */
    void disable() override;

    /**
     * @brief Motor enable function
     *
     */
    void enable() override;

    // hardware variables
    int pwmA_h, pwmA_l;         /*!< phase A pwm pin number */
    int pwmB_h, pwmB_l;         /*!< phase B pwm pin number */
    int pwmC_h, pwmC_l;         /*!< phase C pwm pin number */
    int enable_pin;             /*!< enable pin number */
    int mcpwm_group_id;         /*!< mcpwm group id */
    bool enable_active_high = true;
    float dead_zone;            /*!< a percentage of dead-time(zone) (both high and low side in low) for each pwm cycle [0,1] */
    PhaseState phase_state[3];  /*!< phase state (active / disabled) */

    /**
     * @brief Set voltage to the pwm pin
     *
     * @param Ua phase A voltage
     * @param Ub phase B voltage
     * @param Uc phase C voltage
     */
    void setPwm(float Ua, float Ub, float Uc) override;

    /**
     * @brief Set the phase state
     *
     * @param sa phase A state
     * @param sb phase B state
     * @param sc phase C state
     */
    virtual void setPhaseState(PhaseState sa, PhaseState sb, PhaseState sc) override;

    /**
     * @brief PWM generating function
     *
     */
    void halPwmWrite();

private:
    uint32_t mcpwm_period;
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_oper_handle_t oper[3];
    mcpwm_cmpr_handle_t comparator[3];
    mcpwm_gen_handle_t generator[3][2];
};
