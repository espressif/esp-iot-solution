/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pixel.h"
#include "lamp_array_matrix.h"

static const uint8_t _NeoPixelSineTable[256] = {
    128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170,
    173, 176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211,
    213, 215, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240,
    241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254,
    254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251,
    250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 238, 237, 235, 234, 232,
    230, 228, 226, 224, 222, 220, 218, 215, 213, 211, 208, 206, 203, 201, 198,
    196, 193, 190, 188, 185, 182, 179, 176, 173, 170, 167, 165, 162, 158, 155,
    152, 149, 146, 143, 140, 137, 134, 131, 128, 124, 121, 118, 115, 112, 109,
    106, 103, 100, 97,  93,  90,  88,  85,  82,  79,  76,  73,  70,  67,  65,
    62,  59,  57,  54,  52,  49,  47,  44,  42,  40,  37,  35,  33,  31,  29,
    27,  25,  23,  21,  20,  18,  17,  15,  14,  12,  11,  10,  9,   7,   6,
    5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,   0,   0,   0,
    0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,   10,  11,
    12,  14,  15,  17,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,  37,
    40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
    79,  82,  85,  88,  90,  93,  97,  100, 103, 106, 109, 112, 115, 118, 121,
    124
};

typedef struct {
    LampColor *pixels;
    uint32_t pixel_cnt;
    NeopixelEffect currentEffect;
    LampColor startingColor;
    led_strip_handle_t handle;
} NeopixelController;

static NeopixelController Controller;

void NeopixelInit(led_strip_handle_t handle, uint32_t pixel_cnt)
{
    Controller.pixels = (LampColor *)calloc(pixel_cnt, sizeof(LampColor));
    Controller.pixel_cnt = pixel_cnt;
    Controller.handle = handle;
}

void NeopixelSetEffect(NeopixelEffect effect, LampColor effectColor)
{
    Controller.currentEffect = effect;
    Controller.startingColor = effectColor;

    for (int i = 0; i < Controller.pixel_cnt; i++) {
        Controller.pixels[i] = effectColor;
    }
}

void NeopixelSetColor(uint16_t lampId, LampColor lampColor)
{
    if (lampId >= Controller.pixel_cnt) {
        return;
    }

    Controller.pixels[lampId] = lampColor;
}

void NeopixelSetColorRange(uint16_t lampIdStart, uint16_t lampIdEnd, LampColor lampColor)
{
    if (lampIdStart >= Controller.pixel_cnt) {
        return;
    }

    for (int i = lampIdStart; i <= lampIdEnd && i < Controller.pixel_cnt; i++) {
        NeopixelSetColor(i, lampColor);
    }
}

void NeopixelSendColors()
{
    for (int i = 0; i < Controller.pixel_cnt; i++) {
        led_strip_set_pixel(Controller.handle, i, Controller.pixels[i].red, Controller.pixels[i].green, Controller.pixels[i].blue);
    }
    led_strip_refresh(Controller.handle);
}

void NeopixelSineWaveStep()
{
    static uint8_t t = 0;

    for (int i = 0; i < Controller.pixel_cnt; i++) {
        uint16_t outputColor;

        outputColor = (Controller.startingColor.red * _NeoPixelSineTable[t]) >> 8;
        Controller.pixels[i].red = outputColor;

        outputColor = (Controller.startingColor.green * _NeoPixelSineTable[t]) >> 8;
        Controller.pixels[i].green = outputColor;

        outputColor = (Controller.startingColor.blue * _NeoPixelSineTable[t]) >> 8;
        Controller.pixels[i].blue = outputColor;
    }

    t++;
}

void NeopixelUpdateEffect()
{
    if (Controller.currentEffect == BLINK) {
        NeopixelSineWaveStep();
    }

    NeopixelSendColors();
}
