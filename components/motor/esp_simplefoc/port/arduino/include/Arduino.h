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
#include "esp_hal_mcpwm.h"

#define NOP() asm volatile("nop")
#define PI 3.14159265358979f

typedef bool boolean;
typedef uint8_t byte;
typedef unsigned int word;

#include "WString.h"
#include "Stream.h"
#include "Print.h"
#include "WCharacter.h"

enum PINMODE
{
    INPUT = 0,
    OUTPUT,
    INPUT_PULLUP,
    INPUT_PULLDOWN,
};

enum PINLEVEL
{
    LOW = 0,
    HIGH,
};

enum PININTERRUPT
{
    RISING = 0, // low->high
    FALLING,    // high->low
    CHANGE,     // high->low or low->high
};

