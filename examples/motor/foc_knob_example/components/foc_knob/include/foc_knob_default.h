/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* default knob modes */
enum {
    MOTOR_UNBOUND_NO_DETENTS,            /*!< motor unbound with no detents */
    MOTOR_UNBOUND_FINE_DETENTS,          /*!< motor unbound with fine detents */
    MOTOR_UNBOUND_COARSE_DETENTS,        /*!< motor unbound with coarse detents */
    MOTOR_BOUND_NO_DETENTS,              /*!< motor bound with no detents */
    MOTOR_BOUND_FINE_DETENTS,            /*!< motor bound with fine detents */
    MOTOR_BOUND_COARSE_DETENTS,          /*!< motor bound with coarse detents */
    MOTOR_ON_OFF_STRONG_DETENTS,         /*!< motor on/off with strong detents */
    MOTOR_MAX_MODES,                     /*!< Max mode */
};

/* The number of default parameters in a list */
extern const int DEFAULT_PARAM_LIST_NUM;

/* default knob parameters */
extern foc_knob_param_t const *default_foc_knob_param_lst[];

#ifdef __cplusplus
}
#endif
