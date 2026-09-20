/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "handoff_lvgl_screen.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_cycle_label;

static void bar_anim_exec(void *obj, int32_t value)
{
    lv_bar_set_value((lv_obj_t *)obj, value, LV_ANIM_OFF);
}

static void set_cycle_label(int cycle, int cycles)
{
    if (s_cycle_label) {
        lv_label_set_text_fmt(s_cycle_label, "cycle %d/%d", cycle, cycles);
    }
}

void handoff_lvgl_screen_create(lv_display_t *display, int cycle, int cycles)
{
    /* Warm maroon background: clearly distinct from the GSP hello
     * scene's dark blue (#101018). */
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x481C1C), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
#if LV_VERSION_CHECK(9, 6, 0)
    lv_obj_set_scrollable(s_screen, false);
#else
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
#endif

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "LVGL exclusive");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *bar = lv_bar_create(s_screen);
    lv_obj_set_size(bar, lv_pct(70), 22);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x682828),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE0A040),
                              LV_PART_INDICATOR);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, bar);
    lv_anim_set_values(&anim, 0, 100);
    lv_anim_set_duration(&anim, 1500);
    lv_anim_set_playback_duration(&anim, 1500);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, bar_anim_exec);
    lv_anim_start(&anim);

    s_cycle_label = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_cycle_label, lv_color_hex(0xE8C8C8), 0);
    lv_obj_align(s_cycle_label, LV_ALIGN_BOTTOM_MID, 0, -24);
    set_cycle_label(cycle, cycles);

    /* esp_lv_present creates the only display, which is the default one. */
    (void)display;
    lv_screen_load(s_screen);
}

void handoff_lvgl_screen_delete(void)
{
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
        s_cycle_label = NULL;
    }
}
