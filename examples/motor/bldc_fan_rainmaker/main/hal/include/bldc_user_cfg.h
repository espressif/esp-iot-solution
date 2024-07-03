/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BLDC_LEDC 0
#define BLDC_MCPWM 1

/**
 * @brief Configure the generation of PWM.
 *
 */
#define PWM_MODE (BLDC_MCPWM) /*!< Configure the generation of PWM. */

/**
 * @brief MCPWM Settings
 *
 */
#define MCPWM_CLK_SRC (40 * 1000 * 1000) /*!< Number of count ticks within a period time_us = 1,000,000 / MCPWM_CLK_SRC */
#define MCPWM_PERIOD (2000) /*!< pwm_cycle_us = 1,000,000 / MCPWM_CLK_SRC * MCPWM_PERIOD */

/**
 * @brief LEDC Settings
 *
 */
#define FREQ_HZ (20*1000) /*<! Frequency in Hertz */
#define DUTY_RES (11)     /*!< Set duty resolution to 11 bits */

/**
 * @brief No changes should be made here.
 *
 */
#if PWM_MODE
#define ALARM_COUNT_US (1000 * 1000  * MCPWM_PERIOD / MCPWM_CLK_SRC)
#define DUTY_MAX (MCPWM_PERIOD/2)
#else
#define ALARM_COUNT_US ( 1000*1000 / FREQ_HZ)
#define DUTY_MAX ((1 << DUTY_RES) - 1)
#endif

#define PWM_DUTYCYCLE_05  (DUTY_MAX/20)
#define PWM_DUTYCYCLE_10  (DUTY_MAX/10)
#define PWM_DUTYCYCLE_15  (DUTY_MAX*3/20)
#define PWM_DUTYCYCLE_20  (DUTY_MAX/5)
#define PWM_DUTYCYCLE_25  (DUTY_MAX*5/20)
#define PWM_DUTYCYCLE_30  (DUTY_MAX*3/10)
#define PWM_DUTYCYCLE_40  (DUTY_MAX*2/5)
#define PWM_DUTYCYCLE_50  (DUTY_MAX/2)
#define PWM_DUTYCYCLE_60  (DUTY_MAX*3/5)
#define PWM_DUTYCYCLE_80  (DUTY_MAX*4/5)
#define PWM_DUTYCYCLE_90  (DUTY_MAX*9/10)
#define PWM_DUTYCYCLE_100 (DUTY_MAX)

/**
 * @brief Pulse injection-related parameters.
 * @note Used to detect the initial phase of the motor; MCPWM peripheral support is necessary.
 */
#if PWM_MODE
#define INJECT_ENABLE (0)              /*!< Whether to enable pulse injection. */
#endif
#define INJECT_DUTY (PWM_DUTYCYCLE_90) /*!< Injected torque. */
#define CHARGE_TIME (5)                /*!< Capacitor charging and injection time. */

/**
 * @brief Parameters related to motor alignment.
 *        Used to lock the motor in a specific phase
 *        before strong dragging.
 */
#define ALIGNMENTNMS  (500)              /*!< Duration of alignment, too short may not reach the position, too long may cause the motor to overheat. */
#define ALIGNMENTDUTY (PWM_DUTYCYCLE_20) /*!< alignment torque. */

/**
 * @brief Setting parameters for strong dragging. The principle of
 *        strong dragging is to increase the control frequency and intensity
 * @note  If the control cycle speeds up, corresponding reductions
 *        should be made to the RAMP_TIM_STA, RAMP_TIM_END, RAMP_TIM_STEP
 */
#define RAMP_TIM_STA (1500)               /*!< The start step time for climbing. A smaller value results in faster startup but may lead to overcurrent issues. */
#define RAMP_TIM_END (180)                /*!< The end step time for climbing, adjusted based on the load. If loaded, this value should be relatively larger. */
#define RAMP_TIM_STEP (15)                /*!< Decremental increment for climbing step time—adjusted in accordance with RAMP_TIM_STA. */
#define RAMP_DUTY_STA (PWM_DUTYCYCLE_15) /*!< The starting torque for climbing. */
#define RAMP_DUTY_END (PWM_DUTYCYCLE_40) /*!< The ending torque for climbing. */
#define RAMP_DUTY_INC (13)               /*!< The incremental torque step for climbing—too small a value may result in failure to start, while too large a value may lead to overcurrent issues. */

/**
 * @brief ADC parameters for zero-crossing detection; please do not delete if not in use.
 *
 */
#define ENTER_CLOSE_TIME (10000)        /*!< Enter the closed-loop state delay times. */
#define ZERO_REPEAT_TIME (2)            /*!< Change phase after detecting zero-crossing signals continuously for several times. */
#define AVOID_CONTINUE_CURRENT_TIME (4) /*!< Avoiding Continuous Current  */

/**
 * @brief Comparator parameters for zero-crossing detection; please do not delete if not in use.
 *
 */
#define ZERO_STABLE_FLAG_CNT (4)             /*!< After stable detection for multiple revolutions, it is considered to enter a sensorless state. */
#define ZERO_CROSS_DETECTION_ACCURACY 0xFFFFF /*!< Count a valid comparator value every consecutive detection for how many times. */

/**
 * @brief Common parameter for compensated commutation time calculation
 *
 */
#define ZERO_CROSS_ADVANCE (10)               /*!< Advance switching at zero-crossing, switching angle = 180°/ZERO_CROSS_ADVANCE. angle compensation should be provided. >= 6 */

/**
 * @brief Motor parameter settings.
 *
 */
#define POLE_PAIR (5)      /*!< Number of pole pairs in the motor. */
#define BASE_VOLTAGE (12)  /*!< Rated voltage. */
#define BASE_SPEED (1000)  /*!< Rated speed unit: rpm. */

/**
 * @brief Closed-loop PID parameters for speed.
 *
 */
#define SPEED_KP (0.003)                         /*!< P */
#define SPEED_KI (0.0001)                       /*!< I */
#define SPEED_KD (0)                             /*!< D */
#define SPEED_MIN_INTEGRAL (- DUTY_MAX/SPEED_KI) /*!< Minimum integral saturation limit. */
#define SPEED_MAX_INTEGRAL (DUTY_MAX/SPEED_KI)   /*!< Maximum integral saturation limit. */
#define SPEED_MIN_OUTPUT (PWM_DUTYCYCLE_20)                     /*!< Minimum PWM duty cycle output. */
#define SPEED_MAX_OUTPUT (DUTY_MAX)              /*!< Maximum PWM duty cycle output. */
#define SPEED_CAL_TYPE (0)                       /*!< 0 Incremental 1 Positional */

/**
 * @brief Speed parameter settings.
 *
 */
#define SPEED_MAX_RPM 1000               /*!< Maximum speed. */
#define SPEED_MIN_RPM 0                  /*!< Minimum speed. */
#define MAX_SPEED_MEASUREMENT_FACTOR 1.2 /*!< Supports a measured speed range from 0 to 1.2 times SpeedMAX. Large values could prevent proper filtering of incorrect data. */

#ifdef __cplusplus
}
#endif
