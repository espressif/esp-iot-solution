/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "foc_knob.h"
#include "foc_knob_default.h"

/* arrays of knob parameters for different knob modes */

static const foc_knob_param_t unbound_no_detents[] = {
    {0, 0, 1 * PI / 180, 1, 0, 1.1, "Unbounded\nNo detents"},
};

static const foc_knob_param_t unbound_fine_detents[] = {
    {0, 0, 5 * PI / 180, 0.8, 0, 1.1, "Unbounded\nFine detents"},
};

static const foc_knob_param_t unbound_coarse_detents[] = {
    {0, 0, 10 * PI / 180, 1.2, 0, 1.1, "Unbounded\nStrong detents"},
};

static const foc_knob_param_t bound_no_detents[] = {
    {360, 180, 1 * PI / 180, 0.3, 1, 1.1, "Bounded\nNo detents"},
};

static const foc_knob_param_t bound_fine_dents[] = {
    {32, 0, 5 * PI / 180, 0.6, 1, 1.1, "Bounded\nFine detents"},
};

static const foc_knob_param_t bound_coarse_dents[] = {
    {13, 0, 10 * PI / 180, 1.1, 1, 0.55, "Bounded\nStrong detents"},
};

static const foc_knob_param_t on_off_strong_dents[] = {
    { 2, 0, 60 * PI / 180, 1, 1, 0.55, "On/Off strong detents"},
};

/* Initialize an array of pointers to these parameter arrays for easy access */
foc_knob_param_t const *default_foc_knob_param_lst[] = {
    [MOTOR_UNBOUND_NO_DETENTS] = unbound_no_detents,                    /*!< Unbounded No detents */
    [MOTOR_UNBOUND_FINE_DETENTS] = unbound_fine_detents,                /*!< Unbounded Fine detents */
    [MOTOR_UNBOUND_COARSE_DETENTS] = unbound_coarse_detents,            /*!< Unbounded Strong detents */
    [MOTOR_BOUND_NO_DETENTS] = bound_no_detents,                        /*!< Bounded No detents */
    [MOTOR_BOUND_FINE_DETENTS] = bound_fine_dents,                      /*!< Bounded Fine detents */
    [MOTOR_BOUND_COARSE_DETENTS] = bound_coarse_dents,                  /*!< Bounded Strong detents */
    [MOTOR_ON_OFF_STRONG_DETENTS] = on_off_strong_dents,                /*!< On/Off strong detents */
    [MOTOR_MAX_MODES] = NULL,
};

/* Calculate the number of elements in the default parameter list */
const int DEFAULT_PARAM_LIST_NUM = (sizeof(default_foc_knob_param_lst) / sizeof(default_foc_knob_param_lst[0]) - 1);
