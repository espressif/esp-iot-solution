/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_hal_gpio.h"

static IoRecord ioRecord[GPIO_NUM_MAX]; // record all gpio config

/**
 * @description: Set GPIO Mode. Arduino style function.
 * @param {uint8_t} pin
 * @param {uint8_t} mode
 * @return {*}
 */
void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin > GPIO_NUM_MAX)
    {
        return;
    }

    ioRecord[pin].number = static_cast<gpio_num_t>(pin);
    ioRecord[pin].conf.pin_bit_mask = (1ULL << pin);
    ioRecord[pin].conf.intr_type = GPIO_INTR_DISABLE; // disable interrupt

    switch (mode)
    {
    case INPUT:
        ioRecord[pin].conf.mode = GPIO_MODE_INPUT;
        ioRecord[pin].conf.pull_up_en = GPIO_PULLUP_DISABLE;
        ioRecord[pin].conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        break;
    case OUTPUT:
        ioRecord[pin].conf.mode = GPIO_MODE_OUTPUT;
        ioRecord[pin].conf.pull_up_en = GPIO_PULLUP_DISABLE;
        ioRecord[pin].conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        break;
    case INPUT_PULLUP:
        ioRecord[pin].conf.mode = GPIO_MODE_INPUT;
        ioRecord[pin].conf.pull_up_en = GPIO_PULLUP_ENABLE;
        ioRecord[pin].conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        break;
    case INPUT_PULLDOWN:
        ioRecord[pin].conf.mode = GPIO_MODE_INPUT;
        ioRecord[pin].conf.pull_up_en = GPIO_PULLUP_DISABLE;
        ioRecord[pin].conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        break;
    default:
        break;
    }

    gpio_config(&ioRecord[pin].conf);
}

/**
 * @description: Write GPIO Value. Arduino style function.
 * @param {uint8_t} pin
 * @param {uint8_t} val
 * @return {*}
 */
void digitalWrite(uint8_t pin, uint8_t val)
{
    if (pin > GPIO_NUM_MAX)
    {
        return;
    }
    gpio_set_level(ioRecord[pin].number, val);
}

/**
 * @description: Read GPIO Value. Arduino style function.
 * @param {uint8_t} pin
 * @return {*}
 */
int digitalRead(uint8_t pin)
{
    return gpio_get_level(ioRecord[pin].number);
}

/**
 * @description:  Set GPIO Interrupt. Arduino style function, only return pin number, interrupt process in function <attachInterrupt>
 * @param {uint8_t} pin
 * @return {*}
 */
uint8_t digitalPinToInterrupt(uint8_t pin)
{
    return pin;
}

/**
 * @description: Bind funtion to GPIO Interrupt. Arduino style function.
 * @param {uint8_t} pin
 * @return {*}
 */
void attachInterrupt(uint8_t pin, void (*handler)(void), int mode)
{
    if (pin > GPIO_NUM_MAX)
    {
        return;
    }

    switch (mode)
    {
    case RISING:
        ioRecord[pin].conf.intr_type = GPIO_INTR_POSEDGE;
        break;
    case FALLING:
        ioRecord[pin].conf.intr_type = GPIO_INTR_NEGEDGE;
        break;
    case CHANGE:
        ioRecord[pin].conf.intr_type = GPIO_INTR_ANYEDGE;
        break;
    default:
        break;
    }

    gpio_config(&ioRecord[pin].conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(ioRecord[pin].number, gpio_isr_t(handler), (void *)ioRecord[pin].number);
}