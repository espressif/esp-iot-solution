/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "foc_knob.h"
#include "foc_knob_default.h"

/* arrays of knob parameters for different knob modes */
static const knob_param_t unbound_fine_dents[] = {
    {0, 0, 5 * PI / 180, 0.7, 0, 1.1, "Fine values with detents"},
};

static const knob_param_t unbound_no_dents[] = {
    {0, 0, 1 * PI / 180, 0, 0, 1.1, "Unbounded No detents"},
};

static const knob_param_t super_dial[] = {
    {0, 0, 5 * PI / 180, 2, 0.5, 1.1, "Super Dial"},
};

static const knob_param_t unbound_coarse_dents[] = {
    {0, 0, 10 * PI / 180, 1.1, 0, 1.1, "Fine values with detents unbound"},
};

static const knob_param_t bound_no_dents[] = {
    {360, 180, 1 * PI / 180, 0.05, 0.5, 1.1, "Bounded 0-13 No detents"},
};

static const knob_param_t coarse_dents[] = {
    {0, 0, 8.225806452 * PI / 180, 2, 0.5, 1.1, "Coarse values strong detents"},
};

static const knob_param_t fine_no_dents[] = {
    {256, 127, 1 * PI / 180, 0, 0.5, 1.1, "Fine values no detents"},
};

static const knob_param_t on_off_strong_dents[] = {
    {2, 0, 60 * PI / 180, 1, 0.5, 0.55, "On/Off strong detents"},
};

/* Initialize an array of pointers to these parameter arrays for easy access */
knob_param_t const *default_knob_param_lst[] = {
    [MOTOR_UNBOUND_FINE_DETENTS] = unbound_fine_dents,                //1.Fine values with detents
    [MOTOR_UNBOUND_NO_DETENTS] = unbound_no_dents,                    //2.Unbounded No detents
    [MOTOR_SUPER_DIAL] = super_dial,                                  //3.Super Dial
    [MOTOR_UNBOUND_COARSE_DETENTS] = unbound_coarse_dents,            //4.Fine values with detents unbound
    [MOTOR_BOUND_0_12_NO_DETENTS] = bound_no_dents,                   //5.Bounded 0-13 No detents
    [MOTOR_COARSE_DETENTS] = coarse_dents,                            //6.Coarse values strong detents
    [MOTOR_FINE_NO_DETENTS] = fine_no_dents,                          //7.Fine values no detents
    [MOTOR_ON_OFF_STRONG_DETENTS] = on_off_strong_dents,              //8.On/Off strong detents"
    [MOTOR_MAX_MODES] = NULL,
};

/* Calculate the number of elements in the default parameter list */
const int DEFAULT_PARAM_LIST_NUM = (sizeof(default_knob_param_lst) / sizeof(default_knob_param_lst[0]) - 1);
