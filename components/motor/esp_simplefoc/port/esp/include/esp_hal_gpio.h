/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "Arduino.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _IoRecord
    {
        gpio_config_t conf;
        gpio_num_t number;
    } IoRecord;

    /**
     * @description: Set GPIO Mode. Arduino style function.
     * @param {uint8_t} pin
     * @param {uint8_t} mode
     * @return {*}
     */
    void pinMode(uint8_t pin, uint8_t mode);

    /**
     * @description: Write GPIO Value. Arduino style function.
     * @param {uint8_t} pin
     * @param {uint8_t} val
     * @return {*}
     */
    void digitalWrite(uint8_t pin, uint8_t val);

    /**
     * @description: Read GPIO Value. Arduino style function.
     * @param {uint8_t} pin
     * @return {*}
     */
    int digitalRead(uint8_t pin);

    /**
     * @description: Set GPIO Interrupt. Arduino style function.
     * @param {uint8_t} pin
     * @return {*}
     */
    uint8_t digitalPinToInterrupt(uint8_t pin);

    /**
     * @description: Bind funtion to GPIO Interrupt. Arduino style function.
     * @param {uint8_t} pin
     * @param {int} mode
     * @return {*}
     */
    void attachInterrupt(uint8_t pin, void (*handler)(void), int mode);

#ifdef __cplusplus
}
#endif
