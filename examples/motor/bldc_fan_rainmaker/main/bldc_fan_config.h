/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Bldc pins configuration
#define UPPER_SWITCH_PIN_U           17
#define UPPER_SWITCH_PIN_V           16
#define UPPER_SWITCH_PIN_W           15
#define LOWER_SWITCH_PIN_U           12
#define LOWER_SWITCH_PIN_V           11
#define LOWER_SWITCH_PIN_W           10
#define COMPARER_PIN_U               3
#define COMPARER_PIN_V               46
#define COMPARER_PIN_W               9

// Button pins configuration
#define SETTING_START_PIN            41
#define SETTING_MODE_PIN             40
#define SETTING_SHACKING_HEAD_PIN    39
#define SETTING_TIMER_PIN            38

// Stepper pins configuration
#define STEPPER_A_PIN                14
#define STEPPER_B_PIN                21
#define STEPPER_C_PIN                47
#define STEPPER_D_PIN                48

// Timing Parameter Configuration
#define MAXINUM_TIMING_COUNT         5
#define SIGNLE_TIMING_HOUR_DURACTION 2

// Bldc Running Configuration
#define BLDC_MAX_SPEED               1000
#define BLDC_MIN_SPEED               300
#define BLDC_MID_SPEED               (int)(BLDC_MIN_SPEED + (BLDC_MAX_SPEED - BLDC_MIN_SPEED) / 3)
#define BLDC_HIGH_SPEED              (int)(BLDC_MIN_SPEED + (BLDC_MAX_SPEED - BLDC_MIN_SPEED) * 2 / 3)
#define BLDC_MAX_RESTART_COUNT       3

#ifdef __cplusplus
}
#endif
