/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "esp_hal_misc.h"
#include "esp_hal_serial.h"
#include "esp_hal_gpio.h"

#define NOP() asm volatile("nop")
#define PI 3.14159265358979f

typedef bool boolean;
typedef uint8_t byte;
typedef unsigned int word;

class __FlashStringHelper;
#define F(string_literal) (((string_literal)))

enum PINMODE {
    INPUT = 0,      /*!< Pin mode: input mode */
    OUTPUT,         /*!< Pin mode: output mode */
    INPUT_PULLUP,   /*!< Pin mode: input mode, enable pullup */
    INPUT_PULLDOWN, /*!< Pin mode: input mode, enable pulldown */
};

enum PINLEVEL {
    LOW = 0, /*!< Pin level: low level*/
    HIGH,    /*!< Pin level: high level*/
};

enum PININTERRUPT {
    RISING = 0, /*!< Pin interrupt trigger mode: rising */
    FALLING,    /*!< Pin interrupt trigger mode: falling */
    CHANGE,     /*!< Pin interrupt trigger mode: rising or falling */
};

/**
 * @brief Judge whether it is a number or not.
 *
 * @param c character to be checked, casted to an int, or eof.
 * @return
 *     - true Digit
 *     - false Otherwise
 */
inline boolean isDigit(int c)
{
    return (isdigit(c) == 0 ? false : true);
}
