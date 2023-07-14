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

    unsigned long micros();
    unsigned long millis();
    void delayMicroseconds(uint32_t us);
    void delay(uint32_t ms);
    float min(float a, float b);

#ifdef __cplusplus
}
#endif

