/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *bitmap;
    uint16_t width;
    uint16_t height;
} esp_painter_basic_font_t;

#define ESP_PAINTER_FONT_DECLARE(font_name) extern const esp_painter_basic_font_t font_name

#if CONFIG_ESP_PAINTER_BASIC_FONT_12
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_12);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_16
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_16);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_20
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_20);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_24
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_24);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_28
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_28);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_32
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_32);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_36
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_36);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_40
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_40);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_44
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_44);
#endif

#if CONFIG_ESP_PAINTER_BASIC_FONT_48
ESP_PAINTER_FONT_DECLARE(esp_painter_basic_font_48);
#endif

#ifdef __cplusplus
}
#endif
