/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

class __FlashStringHelper;
#define FPSTR(str_pointer) (reinterpret_cast<const __FlashStringHelper *>(str_pointer))
#define F(string_literal) (FPSTR((string_literal)))

#endif

