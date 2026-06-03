/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *bitmap;
    uint16_t width;
    uint16_t height;
} esp_painter_basic_font_t;

#define esp_painter_FONT_DECLARE(font_name) extern const esp_painter_basic_font_t font_name;

#if CONFIG_ESP_PAINTER_BASIC_FONT_12
esp_painter_FONT_DECLARE(esp_painter_basic_font_12);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_16
esp_painter_FONT_DECLARE(esp_painter_basic_font_16);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_20
esp_painter_FONT_DECLARE(esp_painter_basic_font_20);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_24
esp_painter_FONT_DECLARE(esp_painter_basic_font_24);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_28
esp_painter_FONT_DECLARE(esp_painter_basic_font_28);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_32
esp_painter_FONT_DECLARE(esp_painter_basic_font_32);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_36
esp_painter_FONT_DECLARE(esp_painter_basic_font_36);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_40
esp_painter_FONT_DECLARE(esp_painter_basic_font_40);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_44
esp_painter_FONT_DECLARE(esp_painter_basic_font_44);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_48
esp_painter_FONT_DECLARE(esp_painter_basic_font_48);
#endif

#ifdef __cplusplus
}
#endif
