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
    MOTOR_UNBOUND_FINE_DETENTS,            //0: Fine values with detents
    MOTOR_UNBOUND_NO_DETENTS,              //1: Unbounded No detents
    MOTOR_SUPER_DIAL,                      //2: Super Dial
    MOTOR_UNBOUND_COARSE_DETENTS,          //3: Fine values with detents unbound
    MOTOR_BOUND_0_12_NO_DETENTS,           //4: Bounded 0-13 No detents
    MOTOR_COARSE_DETENTS,                  //5: Coarse values strong detents
    MOTOR_FINE_NO_DETENTS,                 //6: Fine values no detents
    MOTOR_ON_OFF_STRONG_DETENTS,           //7: On/Off strong detents
    MOTOR_MAX_MODES,                       //8: Max mode
};

/* The number of default parameters in a list */
extern const int DEFAULT_PARAM_LIST_NUM;

/* default knob parameters */
extern knob_param_t const *default_knob_param_lst[];

#ifdef __cplusplus
}
#endif
