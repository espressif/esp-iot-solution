/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PAINTER_FONTS_H
#define __PAINTER_FONTS_H

#include <stdint.h>

typedef struct {
    const uint8_t *table;
    uint16_t Width;
    uint16_t Height;
} font_t;

extern const font_t Font24;
extern const font_t Font20;
extern const font_t Font16;
extern const font_t Font12;
extern const font_t Font8;

#endif /* __PAINTER_FONTS_H */
