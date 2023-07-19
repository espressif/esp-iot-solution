/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "esp_platform.h"
#include "esp_app_print.h"

size_t Print::write(const uint8_t *buffer, size_t size)
{
    if (buffer == nullptr) {
        return 0;
    }

    size_t n = 0;
    while (size--) {
        n += write(*buffer++);
    }
    return n;
}

size_t Print::write(const char *str)
{
    if (str == nullptr) {
        return 0;
    }
    return write((const uint8_t *)str, strlen(str));
}

size_t Print::write(const char *buffer, size_t size)
{
    return write((const uint8_t *)buffer, size);
}

size_t Print::print(double n, int digits)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(digits) << n;
    std::string str = oss.str();

    return write(str.c_str());
}

size_t Print::println(void)
{
    return print("\r\n");
}

size_t Print::println(double num, int digits)
{
    size_t n = print(num, digits);
    n += println();
    return n;
}

Stream::Stream()
{
    _startMillis = 0;
    _timeout = 0;
}

int Stream::timedRead()
{
    int c;
    _startMillis = millis();
    do {
        c = read();
        if (c >= 0) {
            return c;
        }
    } while (millis() - _startMillis < _timeout);
    return -1;
}

size_t Stream::readBytes(char *buffer, size_t length)
{
    size_t count = 0;
    while (count < length) {
        int c = timedRead();
        if (c < 0) {
            break;
        }
        *buffer++ = (char)c;
        count++;
    }
    return count;
}

size_t Stream::readBytes(uint8_t *buffer, size_t length)
{
    return readBytes((char *)buffer, length);
}

void Stream::setTimeout(unsigned long timeout)
{
    _timeout = timeout;
}

unsigned long Stream::getTimeout()
{
    return _timeout;
}
