/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_lcd.hpp"
#include "ui.h"

/* Re-flow the SquareLine layout (authored for 1024x600) to the actual panel
 * resolution: camera preview on the left, controls in a column on the right. */
static void app_ui_layout(void)
{
    lv_display_t *disp = lv_display_get_default();
    const int32_t scr_w = lv_display_get_horizontal_resolution(disp);

    const int32_t cam_w = 640;
    const int32_t cam_h = 480;

    /* Camera preview: left edge, vertically centered. */
    lv_obj_set_size(ui_CamImage, cam_w, cam_h);
    lv_obj_set_align(ui_CamImage, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(ui_CamImage, 0, 0);

    /* Control column to the right of the preview. */
    const int32_t gap = 8;
    const int32_t col_x = cam_w + gap;
    int32_t ctrl_w = scr_w - col_x - gap;
    if (ctrl_w < 120) {
        ctrl_w = 120;
    } else if (ctrl_w > 200) {
        ctrl_w = 200;
    }
    const int32_t ctrl_h = 80;
    const int32_t vgap = 12;
    int32_t y = vgap;

    lv_obj_set_align(ui_SelectLabel, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(ui_SelectLabel, col_x, y);
    y += 28;

    lv_obj_set_align(ui_ClassRoller, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(ui_ClassRoller, ctrl_w, ctrl_h);
    lv_obj_set_pos(ui_ClassRoller, col_x, y);
    y += ctrl_h + vgap;

    lv_obj_set_align(ui_RecordButton, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(ui_RecordButton, ctrl_w, ctrl_h);
    lv_obj_set_pos(ui_RecordButton, col_x, y);
    y += ctrl_h + vgap;

    lv_obj_set_align(ui_RecogButton, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(ui_RecogButton, ctrl_w, ctrl_h);
    lv_obj_set_pos(ui_RecogButton, col_x, y);
}

void app_lcd_init(void)
{
    /* Board-agnostic: each board brings up its own LCD + LVGL behind bsp_display_start(). */
    bsp_display_start();
    bsp_display_brightness_set(100);

    bsp_display_lock(0);
    ui_init();
    app_ui_layout();
    bsp_display_unlock();
}
