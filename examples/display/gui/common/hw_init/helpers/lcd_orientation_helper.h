/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "hw_init.h"

#ifdef __cplusplus
extern "C" {
#endif

void lcd_get_orientation_flags(bool base_swap_xy, bool base_mirror_x, bool base_mirror_y,
                               hw_rotation_t rotation,
                               bool *swap_xy, bool *mirror_x, bool *mirror_y);

#ifdef __cplusplus
}
#endif
