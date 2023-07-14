/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/foc_utils.h"
#include <math.h>
#include "soc/soc_caps.h"
#include "IQmathLib.h"

/**
 * @description: Using fast math sin.
 * @param {float} a
 * @return {*}
 */
float _sin(float a)
{
    _iq iq_radians = _IQ(a);
    _iq result = _IQsin(iq_radians);
    return _IQtoF(result);
}

/**
 * @description: Using fast math cos.
 * @param {float} a
 * @return {*}
 */
float _cos(float a)
{
    _iq iq_radians = _IQ(a);
    _iq result = _IQcos(iq_radians);
    return _IQtoF(result);
}

/**
 * @description: Normalizing radian angle to [0,2PI].
 * @param {float} angle
 * @return {*}
 */
float _normalizeAngle(float angle)
{
    float a = fmod(angle, _2PI);
    return a >= 0 ? a : (a + _2PI);
}

/**
 * @description: Electrical angle calculation.
 * @param {float} number
 * @return {*}
 */
float _electricalAngle(float shaft_angle, int pole_pairs)
{
    return (shaft_angle * pole_pairs);
}

/**
 * @description: Using fast math sqrt
 * @param {float} number
 * @return {*}
 */
float _sqrtApprox(float number)
{
    _iq iq_x = _IQ(number);
    _iq iq_result = _IQsqrt(iq_x);
    return _IQtoF(iq_result);
}