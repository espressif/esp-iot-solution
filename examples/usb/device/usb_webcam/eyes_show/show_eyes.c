/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "show_eyes.h"

LV_IMG_DECLARE(img_open_eyes);
LV_IMG_DECLARE(img_blink_eyes);
LV_IMG_DECLARE(img_close_eyes);
LV_IMG_DECLARE(img_static_eyes);

lv_obj_t *eyes_init()
{
    lv_obj_t *f_img = lv_scr_act();
    lv_obj_clear_flag(f_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *img = lv_gif_create(f_img);
    return img;
}

lv_obj_t *eyes_open(lv_obj_t *img)
{
    bsp_display_lock(0);
    lv_gif_set_src(img, &img_open_eyes);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    bsp_display_unlock();
    return img;
}

lv_obj_t *eyes_blink(lv_obj_t *img)
{
    bsp_display_lock(0);
    lv_gif_set_src(img, &img_blink_eyes);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    bsp_display_unlock();
    return img;
}

lv_obj_t *eyes_close(lv_obj_t *img)
{
    bsp_display_lock(0);
    lv_gif_set_src(img, &img_close_eyes);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    bsp_display_unlock();
    return img;
}

lv_obj_t *eyes_static(lv_obj_t *img)
{
    bsp_display_lock(0);
    lv_img_set_src(img, &img_static_eyes);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    bsp_display_unlock();
    return img;
}
