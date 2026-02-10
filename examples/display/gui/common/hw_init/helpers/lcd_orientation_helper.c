/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lcd_orientation_helper.h"

typedef struct {
    int m00;
    int m01;
    int m10;
    int m11;
} lcd_orientation_matrix_t;

static lcd_orientation_matrix_t matrix_from_flags(bool swap_xy, bool mirror_x, bool mirror_y)
{
    int axis_x_x = 1;
    int axis_x_y = 0;
    int axis_y_x = 0;
    int axis_y_y = 1;

    if (swap_xy) {
        int tmp_x = axis_x_x;
        int tmp_y = axis_x_y;
        axis_x_x = axis_y_x;
        axis_x_y = axis_y_y;
        axis_y_x = tmp_x;
        axis_y_y = tmp_y;
    }

    if (mirror_x) {
        axis_x_x = -axis_x_x;
        axis_x_y = -axis_x_y;
    }
    if (mirror_y) {
        axis_y_x = -axis_y_x;
        axis_y_y = -axis_y_y;
    }

    lcd_orientation_matrix_t matrix = {
        .m00 = axis_x_x,
        .m01 = axis_y_x,
        .m10 = axis_x_y,
        .m11 = axis_y_y,
    };
    return matrix;
}

static lcd_orientation_matrix_t matrix_multiply(lcd_orientation_matrix_t lhs, lcd_orientation_matrix_t rhs)
{
    lcd_orientation_matrix_t result = {
        .m00 = lhs.m00 * rhs.m00 + lhs.m01 * rhs.m10,
        .m01 = lhs.m00 * rhs.m01 + lhs.m01 * rhs.m11,
        .m10 = lhs.m10 * rhs.m00 + lhs.m11 * rhs.m10,
        .m11 = lhs.m10 * rhs.m01 + lhs.m11 * rhs.m11,
    };
    return result;
}

static lcd_orientation_matrix_t matrix_from_rotation(esp_lv_adapter_rotation_t rotation)
{
    switch (rotation) {
    case ESP_LV_ADAPTER_ROTATE_90:
        return matrix_from_flags(true, true, false);
    case ESP_LV_ADAPTER_ROTATE_180:
        return matrix_from_flags(false, true, true);
    case ESP_LV_ADAPTER_ROTATE_270:
        return matrix_from_flags(true, false, true);
    case ESP_LV_ADAPTER_ROTATE_0:
    default:
        return matrix_from_flags(false, false, false);
    }
}

void lcd_get_orientation_flags(bool base_swap_xy, bool base_mirror_x, bool base_mirror_y,
                               esp_lv_adapter_rotation_t rotation,
                               bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    lcd_orientation_matrix_t base = matrix_from_flags(base_swap_xy, base_mirror_x, base_mirror_y);
    lcd_orientation_matrix_t rotation_matrix = matrix_from_rotation(rotation);
    lcd_orientation_matrix_t desired = matrix_multiply(rotation_matrix, base);

    for (int test_swap = 0; test_swap <= 1; ++test_swap) {
        for (int test_mirror_x = 0; test_mirror_x <= 1; ++test_mirror_x) {
            for (int test_mirror_y = 0; test_mirror_y <= 1; ++test_mirror_y) {
                lcd_orientation_matrix_t candidate = matrix_from_flags(test_swap, test_mirror_x, test_mirror_y);
                if (candidate.m00 == desired.m00 &&
                        candidate.m01 == desired.m01 &&
                        candidate.m10 == desired.m10 &&
                        candidate.m11 == desired.m11) {
                    if (swap_xy) {
                        *swap_xy = (bool)test_swap;
                    }
                    if (mirror_x) {
                        *mirror_x = (bool)test_mirror_x;
                    }
                    if (mirror_y) {
                        *mirror_y = (bool)test_mirror_y;
                    }
                    return;
                }
            }
        }
    }

    if (swap_xy) {
        *swap_xy = base_swap_xy;
    }
    if (mirror_x) {
        *mirror_x = base_mirror_x;
    }
    if (mirror_y) {
        *mirror_y = base_mirror_y;
    }
}
