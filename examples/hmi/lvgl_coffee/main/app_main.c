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
#include "board.h"

/* LVGL includes */
#include "lvgl_gui.h"

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


// static lv_anim_t anim[3][10];
/**********************
 *  IMAGE DECLARE
 **********************/
LV_IMG_DECLARE(coffee_bean);
LV_IMG_DECLARE(coffee_cup);
LV_IMG_DECLARE(coffee_flower);

/* Image and txt resource */
const void *btn_img[] = {LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT, LV_SYMBOL_PAUSE};
const void *wp_img[] = {&coffee_bean, &coffee_cup, &coffee_flower};
const char *coffee_type[] = {"RISTRETTO", "ESPRESSO", "AMERICANO"};

typedef enum {
    LV_ANIM_NONE = 0,
    LV_ANIM_FLOAT_TOP,      /*Float from/to the top*/
    LV_ANIM_FLOAT_LEFT,     /*Float from/to the left*/
    LV_ANIM_FLOAT_BOTTOM,   /*Float from/to the bottom*/
    LV_ANIM_FLOAT_RIGHT,    /*Float from/to the right*/
    LV_ANIM_GROW_H,         /*Grow/shrink  horizontally*/
    LV_ANIM_GROW_V,         /*Grow/shrink  vertically*/
} lv_anim_builtin_t;

#define LV_ANIM_IN              0x00    /*Animation to show an object. 'OR' it with lv_anim_builtin_t*/
#define LV_ANIM_OUT             0x80    /*Animation to hide an object. 'OR' it with lv_anim_builtin_t*/
#define LV_ANIM_DIR_MASK        0x80    /*ANIM_IN/ANIM_OUT mask*/


static lv_style_int_t lv_obj_get_true_height(lv_obj_t *obj)
{
    lv_style_int_t v = lv_obj_get_height_margin(obj);
    return v + lv_obj_get_style_shadow_width(obj, LV_OBJ_PART_MAIN);
}
/**
 * Animate an object
 * @param obj pointer to an object to animate
 * @param type type of animation from 'lv_anim_builtin_t'. 'OR' it with ANIM_IN or ANIM_OUT
 * @param time time of animation in milliseconds
 * @param delay delay before the animation in milliseconds
 * @param cb a function to call when the animation is ready
 */
static void lv_obj_animate(lv_obj_t *obj, int type, uint16_t time, uint16_t delay, void (*cb)(lv_anim_t *))
{
    lv_obj_t *par = lv_obj_get_parent(obj);

    /*Get the direction*/
    bool out = (type & LV_ANIM_DIR_MASK) == LV_ANIM_IN ? false : true;
    type = type & (~LV_ANIM_DIR_MASK);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_ready_cb(&a, cb);
    lv_anim_path_t path;
    lv_anim_path_init(&path);
    lv_anim_path_set_cb(&path, lv_anim_path_linear);
    lv_anim_set_path(&a, &path);
    lv_anim_value_t start = 0, end = 0;

    /*Init to ANIM_IN*/
    switch (type) {
    case LV_ANIM_FLOAT_LEFT:
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        start = -lv_obj_get_width_margin(obj);
        end = lv_obj_get_x(obj);
        break;
    case LV_ANIM_FLOAT_RIGHT:
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        start = lv_obj_get_width_margin(par);
        end = lv_obj_get_x(obj);
        break;
    case LV_ANIM_FLOAT_TOP:
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        start = -lv_obj_get_true_height(obj);
        end = lv_obj_get_y(obj);
        break;
    case LV_ANIM_FLOAT_BOTTOM:
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        start = lv_obj_get_true_height(par);
        end = lv_obj_get_y(obj);
        break;
    case LV_ANIM_GROW_H:
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
        start = 0;
        end = lv_obj_get_width_margin(obj);
        break;
    case LV_ANIM_GROW_V:
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_height);
        start = 0;
        end = lv_obj_get_height_margin(obj);
        break;
    case LV_ANIM_NONE:
        lv_anim_set_exec_cb(&a, NULL);
        start = 0;
        end = 0;
        break;
    default:
        break;
    }

    /*Swap start and end in case of ANIM OUT*/
    if (out != false) {
        lv_anim_set_values(&a, end, start);
    } else {
        lv_anim_set_values(&a, start, end);
    }

    lv_anim_start(&a);
}

static void prebtn_action(lv_obj_t *obj, lv_event_t event)
{
    switch (event) {
    case LV_EVENT_PRESSED:
        printf("Pressed\n");
        break;

    case LV_EVENT_SHORT_CLICKED:
        printf("Short clicked\n");
        break;

    case LV_EVENT_CLICKED:
        if (--tab_id < 0) {
            tab_id = 2;
        }
        lv_tabview_set_tab_act(tabview, tab_id, true);
        printf("Clicked\n");
        break;

    case LV_EVENT_LONG_PRESSED:
        printf("Long press\n");
        break;

    case LV_EVENT_LONG_PRESSED_REPEAT:
        printf("Long press repeat\n");
        break;

    case LV_EVENT_RELEASED:
        printf("Released\n");
        break;
    }
}

static void nextbtn_action(lv_obj_t *obj, lv_event_t event)
{
    switch (event) {
    case LV_EVENT_PRESSED:
        printf("Pressed\n");
        break;

    case LV_EVENT_SHORT_CLICKED:
        printf("Short clicked\n");
        break;

    case LV_EVENT_CLICKED:
        if (++tab_id > 2) {
            tab_id = 0;
        }
        lv_tabview_set_tab_act(tabview, tab_id, true);
        printf("Clicked\n");
        break;

    case LV_EVENT_LONG_PRESSED:
        printf("Long press\n");
        break;

    case LV_EVENT_LONG_PRESSED_REPEAT:
        printf("Long press repeat\n");
        break;

    case LV_EVENT_RELEASED:
        printf("Released\n");
        break;
    }
}

static void play_end_callback(lv_anim_t *a)
{
    lv_obj_set_pos(prebtn[tab_id], 15, 30);
    lv_obj_set_pos(nextbtn[tab_id], LV_HOR_RES - 50 - 15, 30);
    lv_obj_align(coffee_label[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, 50);
    lv_obj_align(sweet_slider[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, -10, 100);
    lv_obj_align(weak_sweet[tab_id], sweet_slider[tab_id], LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_align(strong_sweet[tab_id], sweet_slider[tab_id], LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_align(playbtn[tab_id], tab[tab_id], LV_ALIGN_IN_BOTTOM_MID, 0, -15);

    lv_obj_animate(prebtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(nextbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(coffee_label[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(sweet_slider[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(weak_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(strong_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(playbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);

    lv_obj_animate(play_arc[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);
    lv_obj_animate(precent_label[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 400, 0, NULL);

    lv_obj_set_hidden(play_arc[tab_id], true);    /* hide the object */
    lv_obj_set_hidden(precent_label[tab_id], true);
    lv_obj_set_state(tabview, LV_STATE_DEFAULT); /* must not sliding when play animation */
}

static void lv_play_arc_set_endangle(lv_obj_t *obj, lv_coord_t v)
{
    int angle = v;
    lv_arc_set_end_angle(obj, angle);

    char precent[8];
    sprintf(precent, "%3d%%", v * 100 / 360);
    lv_label_set_text(precent_label[tab_id], precent);
}

static void play_move_end_callback(lv_anim_t *anim)
{
    lv_obj_t *obj = (lv_obj_t *)anim->var;
    /* Create an animation to modify progress */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, 3000);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_delay(&a, 500);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_play_arc_set_endangle);
    lv_anim_set_ready_cb(&a, play_end_callback);
    lv_anim_start(&a);
}

static void playbtn_action(lv_obj_t *obj, lv_event_t event)
{
    switch (event) {
    case LV_EVENT_PRESSED:
        printf("Pressed\n");
        break;

    case LV_EVENT_SHORT_CLICKED:
        printf("Short clicked\n");
        break;

    case LV_EVENT_CLICKED:
        lv_obj_set_hidden(play_arc[tab_id], false);    /* hide the object */
        lv_obj_set_hidden(precent_label[tab_id], false);

        lv_obj_animate(prebtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
        lv_obj_animate(nextbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
        lv_obj_animate(coffee_label[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
        lv_obj_animate(sweet_slider[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
        lv_obj_animate(weak_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
        lv_obj_animate(strong_sweet[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);
        lv_obj_animate(playbtn[tab_id], LV_ANIM_FLOAT_TOP | LV_ANIM_OUT, 400, 0, NULL);

        lv_label_set_text(precent_label[tab_id], "  0%");
        lv_arc_set_angles(play_arc[tab_id], 0, 0);
        lv_arc_set_start_angle(play_arc[tab_id], 0);
        lv_obj_align(play_arc[tab_id], tab[tab_id], LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[tab_id])) / 2 + 10);
        lv_obj_align(precent_label[tab_id], play_arc[tab_id], LV_ALIGN_CENTER, 0, 0);
        lv_obj_animate(play_arc[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, play_move_end_callback);
        lv_obj_animate(precent_label[tab_id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, NULL);

        lv_obj_set_state(tabview, LV_STATE_DISABLED ); /* must not sliding when play animation */
        printf("Clicked\n");
        break;

    case LV_EVENT_LONG_PRESSED:
        printf("Long press\n");
        break;

    case LV_EVENT_LONG_PRESSED_REPEAT:
        printf("Long press repeat\n");
        break;

    case LV_EVENT_RELEASED:
        printf("Released\n");
        break;
    }
}

static void create_tab(lv_obj_t *parent, uint8_t wp_img_id, uint8_t coffee_type_id, uint8_t id)
{
    lv_page_set_scrlbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_page_set_scrollable_fit(parent, LV_FIT_PARENT);       /* It must not be automatically sized to allow all children to participate. */

    lv_obj_t *wp = lv_img_create(parent, NULL); /* create wallpaper */
    lv_img_set_src(wp, wp_img[wp_img_id]);      /* set wallpaper image */

    static lv_style_t btn_rel_style;
    static lv_style_t btn_img_style;
    lv_style_init(&btn_rel_style);
    lv_style_init(&btn_img_style);
    lv_style_set_bg_opa(&btn_rel_style, LV_STATE_DEFAULT, LV_OPA_10);
    lv_style_set_border_color(&btn_rel_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_shadow_width(&btn_rel_style, LV_STATE_DEFAULT, LV_DPX(10));
    lv_style_set_shadow_color(&btn_rel_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_shadow_spread(&btn_rel_style, LV_STATE_DEFAULT, LV_DPX(5));

    prebtn[id] = lv_btn_create(parent, NULL); /* Create previous page btn */
    lv_obj_set_size(prebtn[id], 50, 50);
    lv_obj_t *preimg = lv_img_create(prebtn[id], NULL);
    lv_img_set_src(preimg, btn_img[0]);
    lv_obj_set_style_local_image_recolor(preimg, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(prebtn[id], 15, 30);
    lv_obj_set_event_cb(prebtn[id], prebtn_action);
    lv_obj_add_style(prebtn[id], LV_BTN_PART_MAIN, &btn_rel_style);

    nextbtn[id] = lv_btn_create(parent, NULL); /* Create next page btn */
    lv_obj_set_size(nextbtn[id], 50, 50);
    lv_obj_t *nextimg = lv_img_create(nextbtn[id], NULL);
    lv_img_set_src(nextimg, btn_img[2]);
    lv_obj_set_style_local_image_recolor(nextimg, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(nextbtn[id], LV_HOR_RES - 50 - 15, 30);
    lv_obj_set_event_cb(nextbtn[id], nextbtn_action);
    lv_obj_add_style(nextbtn[id], LV_BTN_PART_MAIN, &btn_rel_style);

    playbtn[id] = lv_btn_create(parent, NULL); /* Create a make button */
    lv_obj_set_size(playbtn[id], 70, 70);      /* set object size */
    lv_obj_t *playimg = lv_img_create(playbtn[id], NULL);
    lv_img_set_src(playimg, btn_img[1]);
    lv_obj_set_style_local_image_recolor(playimg, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_align(playbtn[id], parent, LV_ALIGN_IN_BOTTOM_MID, 0, -15); /* Align to parent */
    lv_obj_set_event_cb(playbtn[id], playbtn_action);
    lv_obj_add_style(playbtn[id], LV_BTN_PART_MAIN, &btn_rel_style);

    /* Create a new style for the label */
    static lv_style_t coffee_style;
    lv_style_init(&coffee_style);
    coffee_label[id] = lv_label_create(parent, NULL); /* Create coffee type label */
    lv_label_set_text(coffee_label[id], coffee_type[coffee_type_id]);
    lv_style_set_text_color(&coffee_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&coffee_style, LV_STATE_DEFAULT, &lv_font_montserrat_22); /* Unicode and symbol fonts already assigned by the library */
    lv_obj_add_style(coffee_label[id], LV_OBJ_PART_MAIN, &coffee_style);
    lv_obj_align(coffee_label[id], parent, LV_ALIGN_IN_TOP_MID, 0, 50); /* Align to parent */

    /* Create a bar, an indicator and a knob style */
    static lv_style_t style_bar;
    static lv_style_t style_indic;
    static lv_style_t style_knob;
    lv_style_init(&style_bar);
    lv_style_init(&style_indic);
    lv_style_init(&style_knob);
    lv_style_set_bg_color(&style_bar, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_bg_grad_color(&style_bar, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_radius(&style_bar, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_style_set_border_color(&style_bar, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_border_width(&style_bar, LV_STATE_DEFAULT, 1);
    lv_style_set_bg_opa(&style_bar, LV_STATE_DEFAULT, LV_OPA_20);

    lv_style_set_bg_grad_color(&style_indic, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_bg_color(&style_indic, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_radius(&style_indic, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_style_set_opa_scale(&style_indic, LV_STATE_DEFAULT, 240);

    lv_style_set_opa_scale(&style_knob, LV_STATE_DEFAULT, 240);
    lv_style_set_opa_scale(&style_knob, LV_STATE_PRESSED, LV_OPA_COVER);

    sweet_slider[id] = lv_slider_create(parent, NULL); /* Create a sweetness adjustment slider */
    lv_obj_add_style(sweet_slider[id], LV_SLIDER_PART_BG, &style_bar);
    lv_obj_add_style(sweet_slider[id], LV_SLIDER_PART_INDIC, &style_indic);
    lv_obj_add_style(sweet_slider[id], LV_SLIDER_PART_KNOB, &style_knob);
    lv_obj_set_size(sweet_slider[id], LV_HOR_RES - 140, 15);               /* set object size */
    lv_obj_align(sweet_slider[id], parent, LV_ALIGN_IN_TOP_MID, -10, 100); /* Align to parent */
    lv_slider_set_range(sweet_slider[id], 1, 100);                         /* set slider range */
    lv_slider_set_value(sweet_slider[id], 50, false);                      /* set slider current value */

    static lv_style_t sweet_style;
    lv_style_init(&sweet_style);
    lv_style_set_text_color(&sweet_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    // lv_style_set_text_font(&sweet_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    weak_sweet[id] = lv_label_create(parent, NULL); /* Create weak sweet label */
    lv_label_set_text(weak_sweet[id], "WEAK");
    lv_obj_add_style(weak_sweet[id], LV_OBJ_PART_MAIN, &sweet_style);
    lv_obj_align(weak_sweet[id], sweet_slider[id], LV_ALIGN_OUT_LEFT_MID, -5, 0); /* Align to sweet_slider */

    strong_sweet[id] = lv_label_create(parent, NULL); /* Create strong sweet label */
    lv_label_set_text(strong_sweet[id], "STRONG");
    lv_obj_add_style(strong_sweet[id], LV_OBJ_PART_MAIN, &sweet_style);
    lv_obj_align(strong_sweet[id], sweet_slider[id], LV_ALIGN_OUT_RIGHT_MID, 5, 0); /* Align to sweet_slider */

    static lv_style_t arc_style;
    lv_style_init(&arc_style);
    // lv_style_set_line_color(&arc_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    // lv_style_set_line_width(&arc_style, LV_STATE_DEFAULT, 10);
    lv_style_set_bg_opa(&arc_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_style_set_border_opa(&arc_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    play_arc[id] = lv_arc_create(parent, NULL); /* Create progress disk */
    lv_obj_set_size(play_arc[id], 180, 180);    /* set object size */
    lv_arc_set_bg_angles(play_arc[id], 0, 360);
    lv_obj_add_style(play_arc[id], LV_ARC_PART_BG, &arc_style);
    lv_obj_set_hidden(play_arc[id], true);    /* hide the object */
    lv_obj_align(play_arc[id], parent, LV_ALIGN_IN_TOP_MID, 0, (LV_VER_RES - lv_obj_get_height(play_arc[id])) / 2 + 10); /* Align to parent */

    static lv_style_t precent_label_style;
    lv_style_init(&precent_label_style);
    lv_style_set_text_color(&precent_label_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&precent_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_32);
    precent_label[id] = lv_label_create(parent, NULL); /* Create progress label */
    lv_label_set_text(precent_label[id], "  0%");
    lv_obj_add_style(precent_label[id], LV_OBJ_PART_MAIN, &precent_label_style);
    lv_obj_set_hidden(precent_label[id], true);    /* hide the object */
    lv_obj_align(precent_label[id], parent, LV_ALIGN_IN_BOTTOM_MID, 0, -50); /* Align to parent */

    lv_obj_animate(prebtn[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(nextbtn[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(coffee_label[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(sweet_slider[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(weak_sweet[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(strong_sweet[id], LV_ANIM_FLOAT_TOP | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(playbtn[id], LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 400, 0, NULL);
}

static void littlevgl_coffee(void)
{
    tabview = lv_tabview_create(lv_scr_act(), NULL);

    lv_tabview_set_btns_pos(tabview, LV_TABVIEW_TAB_POS_NONE);
    lv_obj_set_size(tabview, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(tabview, 0, 0);

    tab[0] = lv_tabview_add_tab(tabview, "RIS"); /* add RIS tab */
    tab[1] = lv_tabview_add_tab(tabview, "ESP"); /* add ESP tab */
    tab[2] = lv_tabview_add_tab(tabview, "AME"); /* add AME tab */
    lv_tabview_set_anim_time(tabview, 0);

    create_tab(tab[0], 0, 0, 0); /* Create ristretto coffee selection tab page */
    create_tab(tab[1], 1, 1, 1); /* Create espresso coffee selection tab page */
    create_tab(tab[2], 2, 2, 2); /* Create americano coffee selection tab page */
}

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void)
{
    iot_board_init();
    spi_bus_handle_t spi2_bus = iot_board_get_handle(BOARD_SPI2_ID);

    scr_driver_t lcd_drv;
    touch_panel_driver_t touch_drv;
    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi2_bus,
        .pin_num_cs = BOARD_LCD_SPI_CS_PIN,
        .pin_num_dc = BOARD_LCD_SPI_DC_PIN,
        .clk_freq = BOARD_LCD_SPI_CLOCK_FREQ,
        .swap_data = true,
    };

    scr_interface_driver_t *iface_drv;
    scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);

    scr_controller_config_t lcd_cfg = {
        .interface_drv = iface_drv,
        .pin_num_rst = 18,
        .pin_num_bckl = 23,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .offset_hor = 0,
        .offset_ver = 0,
        .width = 240,
        .height = 320,
        .rotate = SCR_DIR_TBLR,
    };
    scr_find_driver(SCREEN_CONTROLLER_ILI9341, &lcd_drv);
    lcd_drv.init(&lcd_cfg);

    touch_panel_config_t touch_cfg = {
        .interface_spi = {
            .spi_bus = spi2_bus,
            .pin_num_cs = BOARD_TOUCH_SPI_CS_PIN,
            .clk_freq = 10000000,
        },
        .interface_type = TOUCH_PANEL_IFACE_SPI,
        .pin_num_int = -1,
        .direction = TOUCH_DIR_TBLR,
        .width = 240,
        .height = 320,
    };
    touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_XPT2046, &touch_drv);
    touch_drv.init(&touch_cfg);
    touch_drv.calibration_run(&lcd_drv, false);

    /* Initialize LittlevGL GUI */
    lvgl_init(&lcd_drv, &touch_drv);

    /* coffee demo */
    littlevgl_coffee();

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
