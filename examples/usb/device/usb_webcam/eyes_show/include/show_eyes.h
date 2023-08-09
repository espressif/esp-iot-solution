/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "lvgl.h"

lv_obj_t* eyes_init();
lv_obj_t* eyes_open(lv_obj_t* img);
lv_obj_t* eyes_close(lv_obj_t* img);
lv_obj_t* eyes_blink(lv_obj_t* img);
lv_obj_t* eyes_static(lv_obj_t* img);
