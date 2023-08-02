/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Get time in ms since boot.
 *
 * @return number of microseconds since underlying timer has been started
 */

unsigned long micros();

/**
 * @brief Get time in us since boot.
 *
 * @return number of milliseconds since underlying timer has been started
 */
unsigned long millis();

/**
 * @brief Delay us.
 *
 * @param us microsecond
 */
void delayMicroseconds(uint32_t us);

/**
 * @brief Rtos ms delay function
 *
 * @param ms millisecond
 */
void delay(uint32_t ms);

/**
 * @brief Minimum function.
 *
 * @param a numbers that need to be compared
 * @param b numbers that need to be compared
 * @return minimum value
 */
float min(float a, float b);

#ifdef __cplusplus
}
#endif
