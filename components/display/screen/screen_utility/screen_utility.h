/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SCREEN_UTILITY_H_
#define _SCREEN_UTILITY_H_

#include "screen_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Declare screen parameters
 *
 */
typedef struct {
    scr_interface_driver_t *interface_drv;
    uint16_t original_width;
    uint16_t original_height;
    uint16_t width;
    uint16_t height;
    uint16_t offset_hor;
    uint16_t offset_ver;
    scr_dir_t dir;
} scr_handle_t;

void scr_utility_apply_offset(const scr_handle_t *lcd_handle, uint16_t res_hor, uint16_t res_ver, uint16_t *x0, uint16_t *y0, uint16_t *x1, uint16_t *y1);

#ifdef __cplusplus
}
#endif

#endif
