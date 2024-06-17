/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    CW = 0, /*!< Clockwise */
    CCW,    /*!< counterclockwise */
} dir_enum_t;

typedef enum {
    INJECT = 0,   /*!< Pulse injection for confirming initial phase */
    ALIGNMENT,    /*!< Alignment phase for fixing the motor to the initial phase */
    DRAG,         /*!< Strong drag gives the motor some initial velocity */
    CLOSED_LOOP,  /*!< Closed-loop sensorless control */
    BLOCKED,      /*!< Motor blocked */
    STOP,         /*!< Motor stalls */
    FAULT,        /*!< Motor failure */
} control_status_enum_t;

typedef enum {
    PHASE_U = 0,
    PHASE_V,
    PHASE_W,
    PHASE_MAX,
} phase_enum_t;

typedef enum {
    ALIGNMENT_COMPARER,  /*!< Comparator detects zero crossing */
#if CONFIG_SOC_MCPWM_SUPPORTED
    ALIGNMENT_ADC,       /*!< ADC detects zero crossing */
#endif
} alignment_mode_t;

/**
 * @brief Some parameters in the control process are akin to global variables.
 *
 */
typedef struct {
    /* motor control */
    dir_enum_t dir;                   /*!< directional */
    control_status_enum_t status;     /*!< Motor status */
    uint8_t phase_cnt;                /*!< Motor current phase */
    uint8_t phase_cnt_prev;           /*!< Motor previous phase */
    uint8_t phase_change_done;        /*!< Motor phase change successful */
    uint16_t duty;                    /*!< duty cycle */
    uint16_t duty_max;                /*!< duty cycle max */
    int16_t drag_time;                /*!< drag time */
    alignment_mode_t alignment_mode;  /*!< alignment mode */
    /* speed */
    uint32_t expect_speed_rpm;        /*!< Expected speed */
    uint32_t speed_rpm;               /*!< Current speed */
    uint32_t speed_count;             /*!< Speed count */
    /* data */
    uint16_t filter_delay;            /*!< delayed count */
    uint16_t filter_failed_count;     /*!< Filter Instability Count */
    int error_hall_count;             /*!< Incorrect phase counting */
    /* inject */
    uint16_t charge_time;             /*!< Capacitor charge time */
    uint8_t inject_adc_read;          /*!< Read adc flag on injection */
    uint8_t inject_count;             /*!< Injection count, 0: not yet started 1-6: injection in progress 7: injection complete */
    uint32_t inject_adc_value[6];     /*!< The adc value at the time of injection */
    /* adc */
    uint32_t adc_value[3];            /*!< adc value for closed-loop control */
    phase_enum_t adc_bemf_phase;      /*!< Current phase current that should be detected */
} control_param_t;

#ifdef __cplusplus
}
#endif
