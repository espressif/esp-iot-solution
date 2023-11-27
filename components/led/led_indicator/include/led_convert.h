/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MAX_HUE 360
#define MAX_SATURATION 255
#define MAX_BRIGHTNESS 255
#define MAX_INDEX 127

#define SET_RGB(r, g, b) \
        ((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

#define SET_IRGB(index, r, g, b) \
        ((((index) & 0x7F) << 25) | SET_RGB(r, g, b))

#define SET_HSV(h, s, v) \
        ((((h) > 360 ? 360 : (h)) & 0x1FF) << 16) | (((s) & 0xFF) << 8) | ((v) & 0xFF)

#define SET_IHSV(index, h, s, v) \
        ((((index) & 0x7F) << 25) | SET_HSV(h, s, v))

#define INSERT_INDEX(index, brightness) \
        ((((index) & 0x7F) << 25) | ((brightness) & 0xFF))

#define SET_INDEX(variable, value) \
        variable = (variable & 0x1FFFFFF) | (((value) & 0x7F) << 25)

#define SET_HUE(variable, value) \
        variable = (variable & 0xFE00FFFF) | (((value) & 0x1FF) << 16)

#define SET_SATURATION(variable, value) \
        variable = (variable & 0xFFFF00FF) | (((value) & 0xFF) << 8)

#define SET_BRIGHTNESS(variable, value) \
        variable = (variable & 0xFFFFFF00) | ((value) & 0xFF)

#define GET_INDEX(variable) \
        ((variable >> 25) & 0x7F)

#define GET_HUE(variable) \
        ((variable >> 16) & 0x1FF)

#define GET_SATURATION(variable) \
        ((variable >> 8) & 0xFF)

#define GET_BRIGHTNESS(variable) \
        (variable & 0xFF)

#define GET_RED(variable) \
        ((variable >> 16) & 0xFF)

#define GET_GREEN(variable) \
        ((variable >> 8) & 0xFF)

#define GET_BLUE(variable) \
        (variable & 0xFF)

/**
 * @brief Convert an RGB color value to an HSV color value.
 *
 * @param rgb_value RGB color value to convert.
 *        R: 0-255, G: 0-255, B: 0-255
 * @return uint32_t HSV color value.
 */
uint32_t led_indicator_rgb2hsv(uint32_t rgb_value);

/**
 * @brief Convert an HSV color value to its RGB components (R, G, B).
 *
 * @param hsv HSV color value.
 *        H: 0-360, S: 0-255, V: 0-255
 * @param r Pointer to store the resulting R component.
 * @param g Pointer to store the resulting G component.
 * @param b Pointer to store the resulting B component.
 */
void led_indicator_hsv2rgb(uint32_t hsv, uint32_t *r, uint32_t *g, uint32_t *b);

#ifdef __cplusplus
}
#endif
