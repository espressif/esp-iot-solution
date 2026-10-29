/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui.h"

void ui_panthera_grasp_init(void)
{
    lv_disp_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp,
                                              lv_color_hex(0x3D9CF0),
                                              lv_color_hex(0xD29922),
                                              true,
                                              LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_RobotGraspScreen_screen_init();
    lv_disp_load_scr(ui_RobotGraspScreen);
}

void ui_panthera_grasp_destroy(void)
{
    ui_RobotGraspScreen_screen_destroy();
}
