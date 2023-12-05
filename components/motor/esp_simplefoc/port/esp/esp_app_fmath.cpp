/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "common/foc_utils.h"
#include "soc/soc_caps.h"
#include "IQmathLib.h"

float _sin(float a)
{
    _iq iq_radians = _IQ(a);
    _iq result = _IQsin(iq_radians);
    return _IQtoF(result);
}

float _cos(float a)
{
    _iq iq_radians = _IQ(a);
    _iq result = _IQcos(iq_radians);
    return _IQtoF(result);
}

float _normalizeAngle(float angle)
{
    float a = fmod(angle, _2PI);
    return a >= 0 ? a : (a + _2PI);
}

float _electricalAngle(float shaft_angle, int pole_pairs)
{
    return (shaft_angle * pole_pairs);
}

float _sqrtApprox(float number)
{
    _iq iq_x = _IQ(number);
    _iq iq_result = _IQsqrt(iq_x);
    return _IQtoF(iq_result);
}
