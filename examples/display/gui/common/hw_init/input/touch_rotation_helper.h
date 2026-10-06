/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "hw_init.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOUCH_ROTATION_STANDARD,
    TOUCH_ROTATION_MIPI_DSI,
} touch_rotation_type_t;

void touch_get_rotation_flags(touch_rotation_type_t type, hw_rotation_t rotation,
                              bool *swap_xy, bool *mirror_x, bool *mirror_y);

#ifdef __cplusplus
}
#endif
