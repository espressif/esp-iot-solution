/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdio.h>
#include "lamp_array_matrix.h"
#include "led_strip.h"

typedef enum {
    HID,
    SOLID,
    BLINK
} NeopixelEffect;

void NeopixelInit(led_strip_handle_t handle, uint32_t pixel_cnt);
void NeopixelSetEffect(NeopixelEffect effect, LampColor effectColor);

void NeopixelSetColor(uint16_t lampId, LampColor lampColor);
void NeopixelSetColorRange(uint16_t lampIdStart, uint16_t lampIdEnd, LampColor lampColor);

void NeopixelSendColors();
void NeopixelUpdateEffect();

#endif
