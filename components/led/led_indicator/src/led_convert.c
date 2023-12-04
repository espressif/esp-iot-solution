/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <math.h>
#include "led_convert.h"

uint32_t led_indicator_rgb2hsv(uint32_t rgb_value)
{
    uint8_t r = (rgb_value >> 16) & 0xFF;
    uint8_t g = (rgb_value >> 8) & 0xFF;
    uint8_t b = rgb_value & 0xFF;
    uint16_t h, s, v;

    uint8_t minRGB, maxRGB;
    uint8_t delta;

    minRGB = r < g ? (r < b ? r : b) : (g < b ? g : b);
    maxRGB = r > g ? (r > b ? r : b) : (g > b ? g : b);

    v = maxRGB;
    delta = maxRGB - minRGB;

    if (delta == 0) {
        h = 0;
        s = 0;
    } else {
        s = delta * 255 / maxRGB;

        if (r == maxRGB) {
            h = (60 * (g - b) / delta + 360) % 360;
        } else if (g == maxRGB) {
            h = (60 * (b - r) / delta + 120);
        } else {
            h = (60 * (r - g) / delta + 240);
        }
    }
    return (h << 16) | (s << 8) | v;
}

void led_indicator_hsv2rgb(uint32_t hsv, uint32_t *r, uint32_t *g, uint32_t *b)
{
    uint16_t h = (hsv >> 16) & 0x1FF;
    uint8_t s = (hsv >> 8) & 0xFF;
    uint8_t v = hsv & 0xFF;

    uint8_t rgb_max = v;
    uint8_t rgb_min = rgb_max * (255 - s) / 255.0f;

    uint8_t i = h / 60;
    uint8_t diff = h % 60;

    // RGB adjustment amount by hue
    uint8_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}
