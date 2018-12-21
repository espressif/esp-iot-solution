// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* C includes */
#include <stdio.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/* LVGL includes */
#include "iot_lvgl.h"

/* ESP32 includes */
#include "esp_log.h"

/**********************
 *      MACROS
 **********************/
#define TAG "coffee_lvgl"
#define LV_ANIM_RESOLUTION 1024
#define LV_ANIM_RES_SHIFT 10

/**********************
 *  STATIC VARIABLES
 **********************/
/* TabView Object */
static int8_t tab_id = 0;
static lv_obj_t *tabview = NULL;
static lv_obj_t *tab[3] = {NULL};

/* Button Object */
static lv_obj_t *prebtn[3] = {NULL};
static lv_obj_t *nextbtn[3] = {NULL};
static lv_obj_t *playbtn[3] = {NULL};

/* Label and slider Object */
static lv_obj_t *coffee_label[3] = {NULL};
static lv_obj_t *sweet_slider[3] = {NULL};
static lv_obj_t *weak_sweet[3] = {NULL};
static lv_obj_t *strong_sweet[3] = {NULL};

static lv_obj_t *play_arc[3] = {NULL};
static lv_obj_t *precent_label[3] = {NULL};

/**********************
 *  IMAGE DECLARE
 **********************/
LV_IMG_DECLARE(coffee_bean);
LV_IMG_DECLARE(coffee_cup);
LV_IMG_DECLARE(coffee_flower);

/* Image and txt resource */
const void *btn_img[] = {SYMBOL_PREV, SYMBOL_PLAY, SYMBOL_NEXT, SYMBOL_PAUSE};
const void *wp_img[] = {&coffee_bean, &coffee_cup, &coffee_flower};
const char *coffee_type[] = {"RISTRETTO", "ESPRESSO", "AMERICANO"};

static lv_res_t prebtn_action(lv_obj_t *btn)
{
    if (--tab_id < 0) {
        tab_id = 2;
    }
    lv_tabview_set_tab_act(tabview, tab_id, true);
    return LV_RES_OK;
}

static lv_res_t nextbtn_action(lv_obj_t *btn)
{
    if (++tab_id > 2) {
        tab_id = 0;
    }
    lv_tabview_set_tab_act(tabview, tab_id, true);
    return LV_RES_OK;
}

static void play_callback(lv_obj_t *obj)
{
    lv_obj_set_pos(prebtn[tab_id], 15, 30);
    lv_obj_set_pos(nextbtn[tab_id], LV_HOR_RES - 50 - 15, 30);
    lv_obj_align(coffee_label[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, 50);
    lv_obj_align(sweet_slider[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, -10, 100);
    lv_obj_align(weak_sweet[tab_id], sweet_slider[tab_id], LV_ALIGN_OUT_LEFT_MID, 0, 0);
    lv_obj_align(strong_sweet[tab_id], sweet_slider[tab_id], LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_align(playbtn[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, 170);

    lv_obj_animate(prebtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(nextbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(coffee_label[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(sweet_slider[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(weak_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(strong_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(playbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);

    lv_obj_animate(play_arc[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(precent_label[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);

    lv_tabview_set_sliding(tabview, true); /* must not sliding when play animation */
}

static void lv_play_arc_set_endangle(lv_obj_t *obj, lv_coord_t x)
{
    lv_arc_set_angles(obj, 0, x);
}

static int32_t play_arc_anim_path_linear(const lv_anim_t *a)
{
    /* Calculate the current step */

    uint16_t step;
    if (a->time == a->act_time) {
        step = LV_ANIM_RESOLUTION; /* Use the last value if the time fully elapsed */
    } else {
        step = (a->act_time * LV_ANIM_RESOLUTION) / a->time;
    }

    /* Get the new value which will be proportional to `step`
     * and the `start` and `end` values*/
    int32_t new_value;
    new_value = (int32_t)step * (a->end - a->start);
    new_value = new_value >> LV_ANIM_RES_SHIFT;
    new_value += a->start;

    char precent[5];
    sprintf(precent, "%3d%%", (int)(new_value * 100.0 / 360));
    lv_label_set_text(precent_label[tab_id], precent);

    return new_value;
}

static void play_arc_callback(lv_obj_t *play_arc)
{
    /* Create an animation to modify progress */
    lv_anim_t a;
    a.var = play_arc;
    a.start = 0;
    a.end = 360;
    a.fp = (lv_anim_fp_t)lv_play_arc_set_endangle;
    a.path = play_arc_anim_path_linear;
    a.end_cb = (lv_anim_cb_t)play_callback;
    a.act_time = 0;       /* Negative number to set a delay */
    a.time = 3000;        /* Animate in 3000 ms */
    a.playback = 0;       /* Make the animation backward too when it's ready */
    a.playback_pause = 0; /* Wait before playback */
    a.repeat = 0;         /* Repeat the animation */
    a.repeat_pause = 0;   /* Wait before repeat */
    lv_anim_create(&a);
}
static lv_res_t playbtn_action(lv_obj_t *btn)
{
    lv_obj_animate(prebtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(nextbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(coffee_label[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(sweet_slider[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(weak_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(strong_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(playbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);

    lv_arc_set_angles(play_arc[tab_id], 0, 0);
    lv_label_set_text(precent_label[tab_id], "  0%");
    lv_obj_align(play_arc[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[tab_id])) / 2 + 10);
    lv_obj_align(precent_label[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[tab_id])) / 2 + 80);
    lv_obj_animate(play_arc[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, play_arc_callback);
    lv_obj_animate(precent_label[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, NULL);

    lv_tabview_set_sliding(tabview, false); /* must not sliding when play animation */

    return LV_RES_OK;
}

static void play_arc_first_callback(lv_obj_t *play_arc)
{
    lv_obj_set_hidden(play_arc, false);
}

static void create_tab(lv_obj_t *parent, uint8_t wp_img_id, uint8_t coffee_type_id, uint8_t id)
{
    lv_page_set_sb_mode(parent, LV_SB_MODE_OFF);
    lv_obj_set_protect(parent, LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);
    lv_page_set_scrl_fit(parent, false, false);       /* It must not be automatically sized to allow all children to participate. */
    lv_page_set_scrl_height(parent, LV_VER_RES + 20); /* Set height of the scrollable part of a page */

    lv_obj_t *wp = lv_img_create(parent, NULL); /* create wallpaper */
    lv_img_set_src(wp, wp_img[wp_img_id]);      /* set wallpaper image */

    static lv_style_t btn_rel_style;
    static lv_style_t btn_pr_style;

    prebtn[id] = lv_btn_create(parent, NULL); /* Create previous page btn */
    lv_obj_set_size(prebtn[id], 50, 50);
    lv_obj_t *preimg = lv_img_create(prebtn[id], NULL);
    lv_img_set_src(preimg, btn_img[0]);
    lv_obj_set_pos(prebtn[id], 15, 30);
    lv_btn_set_action(prebtn[id], LV_BTN_ACTION_CLICK, prebtn_action);
    lv_style_copy(&btn_rel_style, lv_btn_get_style(prebtn[id], LV_BTN_STYLE_REL));
    lv_style_copy(&btn_pr_style, lv_btn_get_style(prebtn[id], LV_BTN_STYLE_PR));
    btn_rel_style.body.main_color = LV_COLOR_WHITE;
    btn_pr_style.body.main_color = LV_COLOR_WHITE;
    btn_rel_style.body.grad_color = LV_COLOR_WHITE;
    btn_pr_style.body.grad_color = LV_COLOR_WHITE;
    btn_rel_style.body.border.color = LV_COLOR_WHITE;
    btn_pr_style.body.border.color = LV_COLOR_WHITE;
    btn_rel_style.body.shadow.color = LV_COLOR_WHITE;
    btn_pr_style.body.shadow.color = LV_COLOR_WHITE;
    btn_rel_style.text.color = LV_COLOR_WHITE;
    btn_pr_style.text.color = LV_COLOR_WHITE;
    btn_rel_style.image.color = LV_COLOR_WHITE;
    btn_pr_style.image.color = LV_COLOR_WHITE;
    btn_rel_style.line.color = LV_COLOR_WHITE;
    btn_pr_style.line.color = LV_COLOR_WHITE;

    lv_img_set_style(preimg, &btn_rel_style);
    lv_btn_set_style(prebtn[id], LV_BTN_STYLE_REL, &btn_rel_style);
    lv_btn_set_style(prebtn[id], LV_BTN_STYLE_PR, &btn_pr_style);

    nextbtn[id] = lv_btn_create(parent, NULL); /* Create next page btn */
    lv_obj_set_size(nextbtn[id], 50, 50);
    lv_obj_t *nextimg = lv_img_create(nextbtn[id], NULL);
    lv_img_set_src(nextimg, btn_img[2]);
    lv_obj_set_pos(nextbtn[id], LV_HOR_RES - 50 - 15, 30);
    lv_btn_set_action(nextbtn[id], LV_BTN_ACTION_CLICK, nextbtn_action);

    lv_img_set_style(nextimg, &btn_rel_style);
    lv_btn_set_style(nextbtn[id], LV_BTN_STYLE_REL, &btn_rel_style);
    lv_btn_set_style(nextbtn[id], LV_BTN_STYLE_PR, &btn_pr_style);

    /* Create a new style for the label */
    static lv_style_t coffee_style;
    coffee_label[id] = lv_label_create(parent, NULL); /* Create coffee type label */
    lv_label_set_text(coffee_label[id], coffee_type[coffee_type_id]);
    lv_style_copy(&coffee_style, lv_label_get_style(coffee_label[id]));
    coffee_style.text.color = LV_COLOR_WHITE;
    coffee_style.text.font = &lv_font_dejavu_30; /* Unicode and symbol fonts already assigned by the library */
    lv_label_set_style(coffee_label[id], &coffee_style);
    lv_obj_align(coffee_label[id], parent, LV_ALIGN_IN_TOP_MID, 0, 50); /* Align to parent */

    /* Create a bar, an indicator and a knob style */
    static lv_style_t style_bar;
    static lv_style_t style_indic;
    static lv_style_t style_knob;

    lv_style_copy(&style_bar, &lv_style_pretty);
    style_bar.body.main_color = LV_COLOR_BLACK;
    style_bar.body.grad_color = LV_COLOR_GRAY;
    style_bar.body.radius = LV_RADIUS_CIRCLE;
    style_bar.body.border.color = LV_COLOR_WHITE;
    style_bar.body.opa = LV_OPA_60;
    style_bar.body.padding.hor = 0;
    style_bar.body.padding.ver = LV_DPI / 10;

    lv_style_copy(&style_indic, &lv_style_pretty);
    style_indic.body.grad_color = LV_COLOR_WHITE;
    style_indic.body.main_color = LV_COLOR_WHITE;
    style_indic.body.radius = LV_RADIUS_CIRCLE;
    style_indic.body.shadow.width = LV_DPI / 10;
    style_indic.body.shadow.color = LV_COLOR_WHITE;
    style_indic.body.padding.hor = LV_DPI / 30;
    style_indic.body.padding.ver = LV_DPI / 30;

    lv_style_copy(&style_knob, &lv_style_pretty);
    style_knob.body.radius = LV_RADIUS_CIRCLE;
    style_knob.body.opa = LV_OPA_70;

    sweet_slider[id] = lv_slider_create(parent, NULL); /* Create a sweetness adjustment slider */
    lv_slider_set_style(sweet_slider[id], LV_SLIDER_STYLE_BG, &style_bar);
    lv_slider_set_style(sweet_slider[id], LV_SLIDER_STYLE_INDIC, &style_indic);
    lv_slider_set_style(sweet_slider[id], LV_SLIDER_STYLE_KNOB, &style_knob);
    lv_obj_set_size(sweet_slider[id], LV_HOR_RES - 140, 40);               /* set object size */
    lv_obj_align(sweet_slider[id], parent, LV_ALIGN_IN_TOP_MID, -10, 100); /* Align to parent */
    lv_slider_set_range(sweet_slider[id], 0, 100);                         /* set slider range */
    lv_slider_set_value(sweet_slider[id], 50);                             /* set slider current value */

    static lv_style_t sweet_style;

    weak_sweet[id] = lv_label_create(parent, NULL); /* Create weak sweet label */
    lv_label_set_text(weak_sweet[id], "WEAK");
    lv_style_copy(&sweet_style, lv_label_get_style(weak_sweet[id]));
    sweet_style.text.color = LV_COLOR_WHITE;
    sweet_style.text.font = &lv_font_dejavu_20; /* Unicode and symbol fonts already assigned by the library */
    lv_label_set_style(weak_sweet[id], &sweet_style);
    lv_obj_align(weak_sweet[id], sweet_slider[id], LV_ALIGN_OUT_LEFT_MID, 0, 0); /* Align to sweet_slider */

    strong_sweet[id] = lv_label_create(parent, NULL); /* Create strong sweet label */
    lv_label_set_text(strong_sweet[id], "STRONG");
    lv_label_set_style(strong_sweet[id], &sweet_style);
    lv_obj_align(strong_sweet[id], sweet_slider[id], LV_ALIGN_OUT_RIGHT_MID, 0, 0); /* Align to sweet_slider */

    playbtn[id] = lv_btn_create(parent, NULL); /* Create a make button */
    lv_obj_set_size(playbtn[id], 70, 70);      /* set object size */
    lv_obj_t *playimg = lv_img_create(playbtn[id], NULL);
    lv_img_set_src(playimg, btn_img[1]);
    lv_obj_align(playbtn[id], parent, LV_ALIGN_IN_TOP_MID, 0, 170); /* Align to parent */
    lv_btn_set_action(playbtn[id], LV_BTN_ACTION_CLICK, playbtn_action);

    lv_img_set_style(playimg, &btn_rel_style);
    lv_btn_set_style(playbtn[id], LV_BTN_STYLE_REL, &btn_rel_style);
    lv_btn_set_style(playbtn[id], LV_BTN_STYLE_PR, &btn_pr_style);
    lv_obj_set_protect(playbtn[id], LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);

    lv_obj_animate(prebtn[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(nextbtn[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(coffee_label[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(sweet_slider[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(weak_sweet[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(strong_sweet[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(playbtn[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, NULL);

    play_arc[id] = lv_arc_create(parent, NULL); /* Create progress disk */
    lv_obj_set_size(play_arc[id], 180, 180);    /* set object size */
    lv_arc_set_angles(play_arc[id], 0, 0);      /* set current angle */

    static lv_style_t arc_style;
    lv_style_copy(&arc_style, lv_arc_get_style(play_arc[id], LV_ARC_STYLE_MAIN));
    arc_style.line.color = LV_COLOR_WHITE;
    arc_style.line.width = 10;
    lv_arc_set_style(play_arc[id], LV_ARC_STYLE_MAIN, &arc_style);
    lv_obj_set_hidden(play_arc[id], true);                                                                               /* hide the object */
    lv_obj_align(play_arc[id], parent, LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[id])) / 2 + 10); /* Align to parent */
    lv_obj_animate(play_arc[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, play_arc_first_callback);                   /* animation of hide the object */

    static lv_style_t precent_label_style;
    precent_label[id] = lv_label_create(parent, NULL); /* Create progress label */
    lv_label_set_text(precent_label[id], "  0%");
    lv_style_copy(&precent_label_style, lv_label_get_style(precent_label[id]));
    precent_label_style.text.color = LV_COLOR_WHITE;
    precent_label_style.text.font = &lv_font_dejavu_40; /* Unicode and symbol fonts already assigned by the library */
    lv_label_set_style(precent_label[id], &precent_label_style);
    lv_obj_align(precent_label[id], parent, LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(precent_label[id])) / 2 + 80); /* Align to parent */
    lv_obj_animate(precent_label[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);                                           /* animation of hide the object */
}

static void tabview_lod_action(lv_obj_t *tabview, uint16_t tab_action_id)
{
    tab_id = tab_action_id;
}

static void littlevgl_coffee(void)
{
    lv_theme_set_current(lv_theme_zen_init(100, NULL));

    tabview = lv_tabview_create(lv_scr_act(), NULL);
    lv_obj_set_size(tabview, LV_HOR_RES + 16, LV_VER_RES + 90);
    lv_obj_set_pos(tabview, -8, -60);

    tab[0] = lv_tabview_add_tab(tabview, "RIS"); /* add RIS tab */
    tab[1] = lv_tabview_add_tab(tabview, "ESP"); /* add ESP tab */
    tab[2] = lv_tabview_add_tab(tabview, "AME"); /* add AME tab */
    lv_tabview_set_tab_load_action(tabview, tabview_lod_action);
    lv_obj_set_protect(tabview, LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);
    lv_tabview_set_anim_time(tabview, 0);

    create_tab(tab[0], 0, 0, 0); /* Create ristretto coffee selection tab page */
    create_tab(tab[1], 1, 1, 1); /* Create espresso coffee selection tab page */
    create_tab(tab[2], 2, 2, 2); /* Create americano coffee selection tab page */
}

static void user_task(void *pvParameter)
{
    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
extern "C" void app_main()
{
    /* Initialize LittlevGL GUI */
    lvgl_init();

    /* coffee demo */
    littlevgl_coffee();

    xTaskCreate(
        user_task,   // Task Function
        "user_task", // Task Name
        1024,        // Stack Depth
        NULL,        // Parameters
        1,           // Priority
        NULL);       // Task Handler

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
