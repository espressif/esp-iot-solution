/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Arduino.h"

#include "Print.h"
extern "C"
{
#include "time.h"
}

size_t Print::write(const uint8_t *buffer, size_t size)
{
    size_t n = 0;
    while (size--)
    {
        n += write(*buffer++);
    }
    return n;
}

size_t Print::printf(const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if (len < 0)
    {
        va_end(arg);
        return 0;
    }
    if (len >= (int)sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            va_end(arg);
            return 0;
        }
        len = vsnprintf(temp, len + 1, format, arg);
    }
    va_end(arg);
    len = write((uint8_t *)temp, len);
    if (temp != loc_buf)
    {
        free(temp);
    }
    return len;
}

size_t Print::print(const char str[])
{
    return write(str);
}

size_t Print::print(char c)
{
    return write(c);
}

size_t Print::print(unsigned char b, int base)
{
    return print((unsigned long)b, base);
}

size_t Print::print(int n, int base)
{
    return print((long)n, base);
}

size_t Print::print(unsigned int n, int base)
{
    return print((unsigned long)n, base);
}

size_t Print::print(long n, int base)
{
    int t = 0;
    if (base == 10 && n < 0)
    {
        t = print('-');
        n = -n;
    }
    return printNumber(static_cast<unsigned long>(n), base) + t;
}

size_t Print::print(unsigned long n, int base)
{
    if (base == 0)
    {
        return write(n);
    }
    else
    {
        return printNumber(n, base);
    }
}

size_t Print::print(long long n, int base)
{
    int t = 0;
    if (base == 10 && n < 0)
    {
        t = print('-');
        n = -n;
    }
    return printNumber(static_cast<unsigned long long>(n), base) + t;
}

size_t Print::print(unsigned long long n, int base)
{
    if (base == 0)
    {
        return write(n);
    }
    else
    {
        return printNumber(n, base);
    }
}

size_t Print::print(double n, int digits)
{
    return printFloat(n, digits);
}

size_t Print::print(struct tm *timeinfo, const char *format)
{
    const char *f = format;
    if (!f)
    {
        f = "%c";
    }
    char buf[64];
    size_t written = strftime(buf, 64, f, timeinfo);
    if (written == 0)
    {
        return written;
    }
    return print(buf);
}

size_t Print::println(void)
{
    return print("\r\n");
}

size_t Print::println(const char c[])
{
    size_t n = print(c);
    n += println();
    return n;
}

size_t Print::println(char c)
{
    size_t n = print(c);
    n += println();
    return n;
}

size_t Print::println(unsigned char b, int base)
{
    size_t n = print(b, base);
    n += println();
    return n;
}

size_t Print::println(int num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(unsigned int num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(long num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(unsigned long num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(long long num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(unsigned long long num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(double num, int digits)
{
    size_t n = print(num, digits);
    n += println();
    return n;
}

size_t Print::println(struct tm *timeinfo, const char *format)
{
    size_t n = print(timeinfo, format);
    n += println();
    return n;
}

size_t Print::printNumber(unsigned long n, uint8_t base)
{
    char buf[8 * sizeof(n) + 1];
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    if (base < 2)
    {
        base = 10;
    }

    do
    {
        char c = n % base;
        n /= base;

        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);

    return write(str);
}

size_t Print::printNumber(unsigned long long n, uint8_t base)
{
    char buf[8 * sizeof(n) + 1];
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    if (base < 2)
    {
        base = 10;
    }

    do
    {
        auto m = n;
        n /= base;
        char c = m - base * n;

        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);

    return write(str);
}

size_t Print::printFloat(double number, uint8_t digits)
{
    size_t n = 0;

    if (isnan(number))
    {
        return print("nan");
    }
    if (isinf(number))
    {
        return print("inf");
    }
    if (number > 4294967040.0)
    {
        return print("ovf");
    }
    if (number < -4294967040.0)
    {
        return print("ovf");
    }

    if (number < 0.0)
    {
        n += print('-');
        number = -number;
    }

    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i)
    {
        rounding /= 10.0;
    }

    number += rounding;

    unsigned long int_part = (unsigned long)number;
    double remainder = number - (double)int_part;
    n += print(int_part);

    if (digits > 0)
    {
        n += print(".");
    }

    while (digits-- > 0)
    {
        remainder *= 10.0;
        int toPrint = int(remainder);
        n += print(toPrint);
        remainder -= toPrint;
    }

    return n;
}
