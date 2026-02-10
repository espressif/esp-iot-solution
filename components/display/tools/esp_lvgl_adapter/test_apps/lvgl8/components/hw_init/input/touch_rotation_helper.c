/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "touch_rotation_helper.h"

void touch_get_rotation_flags(touch_rotation_type_t type, esp_lv_adapter_rotation_t rotation,
                              bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    bool swap = false;
    bool x_mirror, y_mirror;

    if (type == TOUCH_ROTATION_MIPI_DSI) {
        x_mirror = true;
        y_mirror = true;
        switch (rotation) {
        case ESP_LV_ADAPTER_ROTATE_90:
            swap = true;
            x_mirror = false;
            y_mirror = true;
            break;
        case ESP_LV_ADAPTER_ROTATE_180:
            swap = false;
            x_mirror = false;
            y_mirror = false;
            break;
        case ESP_LV_ADAPTER_ROTATE_270:
            swap = true;
            x_mirror = true;
            y_mirror = false;
            break;
        case ESP_LV_ADAPTER_ROTATE_0:
        default:
            swap = false;
            x_mirror = true;
            y_mirror = true;
            break;
        }
    } else {
        x_mirror = false;
        y_mirror = false;
        switch (rotation) {
        case ESP_LV_ADAPTER_ROTATE_90:
            swap = true;
            x_mirror = true;
            y_mirror = false;
            break;
        case ESP_LV_ADAPTER_ROTATE_180:
            swap = false;
            x_mirror = true;
            y_mirror = true;
            break;
        case ESP_LV_ADAPTER_ROTATE_270:
            swap = true;
            x_mirror = false;
            y_mirror = true;
            break;
        case ESP_LV_ADAPTER_ROTATE_0:
        default:
            swap = false;
            x_mirror = false;
            y_mirror = false;
            break;
        }
    }

    if (swap_xy) {
        *swap_xy = swap;
    }
    if (mirror_x) {
        *mirror_x = x_mirror;
    }
    if (mirror_y) {
        *mirror_y = y_mirror;
    }
}
