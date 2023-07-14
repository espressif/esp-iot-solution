/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <ctype.h>

inline boolean isDigit(int c)
{
    return (isdigit(c) == 0 ? false : true);
}

