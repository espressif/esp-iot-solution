/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Arduino.h"
#include "esp_hal_misc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

/**
 * @description: Get time in ms since boot.
 * @return {*}
 */
unsigned long micros()
{
    return (unsigned long)(esp_timer_get_time());
}

/**
 * @description: Get time in us since boot.
 * @return {*}
 */
unsigned long millis()
{
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void delayMicroseconds(uint32_t us)
{
    uint64_t m = (uint64_t)esp_timer_get_time();
    if (us)
    {
        uint64_t e = (m + us);
        if (m > e)
        { // overflow
            while ((uint64_t)esp_timer_get_time() > e)
            {
                NOP();
            }
        }
        while ((uint64_t)esp_timer_get_time() < e)
        {
            NOP();
        }
    }
}

/**
 * @description: Rtos delay function.
 * @param {uint32_t} ms
 * @return {*}
 */
void delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

/**
 * @description: Min function.
 * @param {float} a
 * @param {float} b
 * @return {*}
 */
float min(float a, float b)
{
    if (a < b)
    {
        return a;
    }

    return b;
}