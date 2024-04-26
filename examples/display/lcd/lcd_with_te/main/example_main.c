/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_bsp.h"
#include "display.h"

#include "lv_demos.h"

LV_IMG_DECLARE(icon_launcher_lookwordstrans);
LV_IMG_DECLARE(icon_launcher_phonetictrans);
LV_IMG_DECLARE(icon_launcher_recitewords);
LV_IMG_DECLARE(icon_launcher_oral_trainin);
LV_IMG_DECLARE(icon_launcher_dictation);
LV_IMG_DECLARE(icon_launcher_xiaoai);
LV_IMG_DECLARE(icon_launcher_newwordbook);
LV_IMG_DECLARE(icon_launcher_wordrecords);
LV_IMG_DECLARE(icon_launcher_settings);

const lv_img_dsc_t *map_table[] = {
    &icon_launcher_lookwordstrans,
    &icon_launcher_phonetictrans,
    &icon_launcher_recitewords,
    &icon_launcher_oral_trainin,
    &icon_launcher_dictation,
    &icon_launcher_xiaoai,
    &icon_launcher_newwordbook,
    &icon_launcher_wordrecords,
    &icon_launcher_settings,
};

/**
 * Show an example to scroll snap
 */
void lv_create_scroll()
{
    lv_obj_t *panel = lv_obj_create(lv_scr_act());
    // lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 560, 170);
    lv_obj_set_style_radius(panel, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x00), 0);
    lv_obj_set_scroll_snap_x(panel, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

    uint32_t i;
    for (i = 0; i < 9; i++) {
        lv_obj_t *btn = lv_obj_create(panel);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 100, lv_pct(100));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x00), 0);

        lv_obj_t *icon = lv_img_create(btn);
        lv_img_set_src(icon, map_table[i]);
        lv_obj_center(icon);
    }
    lv_obj_update_snap(panel, LV_ANIM_ON);
}

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
#if CONFIG_BSP_LCD_BOARD_I80_170_560
        .buffer_size = EXAMPLE_LCD_I80_H_RES * EXAMPLE_LCD_I80_V_RES,
#else
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#endif
#if CONFIG_EXAMPLE_DISPLAY_ROTATION_90
        .rotate = LV_DISP_ROT_90,
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_270
        .rotate = LV_DISP_ROT_270,
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_180
        .rotate = LV_DISP_ROT_180,
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_0
        .rotate = LV_DISP_ROT_NONE,
#endif
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
#if CONFIG_EXAMPLE_DISPLAY_SHOW_WIDGET
    lv_demo_widgets();
#else
    lv_create_scroll();
#endif
    bsp_display_unlock();
}
