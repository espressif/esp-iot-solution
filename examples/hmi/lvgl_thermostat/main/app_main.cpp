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
#include "esp_freertos_hooks.h"

/* lvgl includes */
#include "iot_lvgl.h"
#include "lv_calibration.h"

/* ESP32 includes */
#include "esp_log.h"
#include "iot_param.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/**********************
 *      MACROS
 **********************/
#define TAG "thermostat"

/**********************
 *  STATIC VARIABLES
 **********************/
/* LVGL Object */
static lv_obj_t *holder_page;
static lv_obj_t *model_page;
static lv_obj_t *cont_page;
static lv_obj_t *wp;
static lv_obj_t *temperature;
static lv_obj_t *img[3];
static lv_obj_t *slider;

/* Style Object */
static lv_style_t style_wp;
static lv_style_t style_btn_rel;
static lv_style_t style_btn_pr;
static lv_style_t style_btn_tgl_rel;
static lv_style_t style_btn_tgl_pr;
static lv_style_t style_btn_min_plus_rel;

static float temperature_num = 21.5;

/**********************
 *  IMAGE DECLARE
 **********************/
LV_IMG_DECLARE(lv_day);
LV_IMG_DECLARE(lv_night);
LV_IMG_DECLARE(lv_snow);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t day_click(lv_obj_t *btn);
static lv_res_t night_click(lv_obj_t *btn);
static lv_res_t vacation_click(lv_obj_t *btn);
static lv_res_t minus_click(lv_obj_t *btn);
static lv_res_t plus_click(lv_obj_t *btn);
static lv_res_t slider_action(lv_obj_t *slider);

static void littlevgl_thermostat(void)
{
    /*Styles of the buttons*/
    lv_style_copy(&style_btn_rel, &lv_style_btn_rel);
    lv_style_copy(&style_btn_pr, &lv_style_btn_pr);
    lv_style_copy(&style_btn_tgl_rel, &lv_style_btn_tgl_rel);
    lv_style_copy(&style_btn_tgl_pr, &lv_style_btn_tgl_pr);
    lv_style_copy(&style_btn_min_plus_rel, &lv_style_btn_rel);

    style_btn_min_plus_rel.body.opa = LV_OPA_COVER;
    style_btn_rel.body.opa = LV_OPA_COVER;
    style_btn_pr.body.opa = LV_OPA_COVER;
    style_btn_tgl_rel.body.opa = LV_OPA_COVER;
    style_btn_tgl_pr.body.opa = LV_OPA_COVER;

    style_btn_min_plus_rel.body.shadow.width = 0;
    style_btn_rel.body.shadow.width = 0;
    style_btn_pr.body.shadow.width = 0;
    style_btn_tgl_rel.body.shadow.width = 0;
    style_btn_tgl_pr.body.shadow.width = 0;

    style_btn_min_plus_rel.body.border.opa = LV_OPA_0;
    style_btn_rel.body.border.opa = LV_OPA_0;
    style_btn_pr.body.border.opa = LV_OPA_0;
    style_btn_tgl_rel.body.border.opa = LV_OPA_0;
    style_btn_tgl_pr.body.border.opa = LV_OPA_0;

    style_btn_min_plus_rel.body.empty = 1;
    style_btn_rel.body.empty = 1;
    style_btn_pr.body.empty = 1;
    style_btn_tgl_rel.body.empty = 1;
    style_btn_tgl_pr.body.empty = 1;

    style_btn_rel.text.color = LV_COLOR_MAKE(0x8b, 0x89, 0x89);
    style_btn_rel.image.color = LV_COLOR_MAKE(0x8b, 0x89, 0x89);

    /*Style of the wallpaper*/
    lv_style_copy(&style_wp, &lv_style_plain);
    style_wp.image.color = LV_COLOR_RED;

    /*Create a holder page (the page become scrollable on small displays )*/
    holder_page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(holder_page, LV_HOR_RES, LV_VER_RES);
    lv_page_set_style(holder_page, LV_PAGE_STYLE_BG, &lv_style_transp_fit);
    lv_page_set_style(holder_page, LV_PAGE_STYLE_SCRL, &lv_style_transp);
    lv_page_set_scrl_layout(holder_page, LV_LAYOUT_PRETTY);

    /*Create a wallpaper on the page*/
    wp = lv_img_create(holder_page, NULL);
    lv_obj_set_protect(wp, LV_PROTECT_PARENT); /*Don't let to move the wallpaper by the layout */
    lv_obj_set_parent(wp, holder_page);
    lv_obj_set_parent(lv_page_get_scrl(holder_page), holder_page);
    lv_img_set_src(wp, &lv_day);
    lv_obj_set_size(wp, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(wp, 0, 0);
    lv_img_set_style(wp, &style_wp);
    lv_img_set_auto_size(wp, false);

    model_page = lv_page_create(lv_scr_act(), NULL);
    lv_cont_set_fit(model_page, true, true);
    lv_obj_align(model_page, holder_page, LV_ALIGN_IN_TOP_LEFT, 5, -10);
    lv_page_set_style(model_page, LV_PAGE_STYLE_BG, &lv_style_transp_fit);
    lv_page_set_style(model_page, LV_PAGE_STYLE_SCRL, &lv_style_transp);
    lv_page_set_scrl_layout(model_page, LV_LAYOUT_PRETTY);

    /*************** Day ****************/
    cont_page = lv_page_create(model_page, NULL);
    lv_obj_set_height(cont_page, 70);
    lv_page_set_style(cont_page, LV_PAGE_STYLE_BG, &lv_style_transp_fit);
    lv_page_set_style(cont_page, LV_PAGE_STYLE_SCRL, &lv_style_transp);
    lv_cont_set_fit(cont_page, true, false);

    img[0] = lv_img_create(cont_page, NULL);
    lv_img_set_style(img[0], &style_btn_rel);
    lv_img_set_src(img[0], SYMBOL_REFRESH);
    lv_obj_align(img[0], cont_page, LV_ALIGN_IN_LEFT_MID, 5, 0);

    lv_obj_t *btn;
    btn = lv_btn_create(cont_page, NULL);
    lv_page_glue_obj(btn, true);
    lv_cont_set_fit(btn, true, true);
    lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style_btn_rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style_btn_pr);
    lv_btn_set_style(btn, LV_BTN_STYLE_TGL_REL, &style_btn_tgl_rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_TGL_PR, &style_btn_tgl_pr);
    lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, day_click);

    lv_obj_t *btn_l;
    btn_l = lv_label_create(btn, NULL);
    lv_label_set_text(btn_l, "Day");
    lv_obj_align(btn, img[0], LV_ALIGN_OUT_RIGHT_MID, -23, 0);

    /*************** Night ****************/
    cont_page = lv_page_create(model_page, NULL);
    lv_obj_set_height(cont_page, 70);
    lv_page_set_style(cont_page, LV_PAGE_STYLE_BG, &lv_style_transp_fit);
    lv_page_set_style(cont_page, LV_PAGE_STYLE_SCRL, &lv_style_transp);
    lv_cont_set_fit(cont_page, true, false);

    img[1] = lv_img_create(cont_page, NULL);
    lv_img_set_style(img[1], &style_btn_rel);
    lv_img_set_src(img[1], SYMBOL_HOME);
    lv_obj_align(img[1], cont_page, LV_ALIGN_IN_LEFT_MID, 5, 0);

    btn = lv_btn_create(cont_page, btn);
    lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, night_click);
    btn_l = lv_label_create(btn, btn_l);
    lv_label_set_text(btn_l, "Night");
    lv_obj_align(btn, img[1], LV_ALIGN_OUT_RIGHT_MID, -23, 0);

    /*************** snow ****************/
    cont_page = lv_page_create(model_page, NULL);
    lv_obj_set_height(cont_page, 70);
    lv_page_set_style(cont_page, LV_PAGE_STYLE_BG, &lv_style_transp_fit);
    lv_page_set_style(cont_page, LV_PAGE_STYLE_SCRL, &lv_style_transp);
    lv_cont_set_fit(cont_page, true, false);

    img[2] = lv_img_create(cont_page, NULL);
    lv_img_set_style(img[2], &style_btn_rel);
    lv_img_set_src(img[2], SYMBOL_CHARGE);
    lv_obj_align(img[2], cont_page, LV_ALIGN_IN_LEFT_MID, 5, 0);

    btn = lv_btn_create(cont_page, btn);
    lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, vacation_click);
    btn_l = lv_label_create(btn, btn_l);
    lv_label_set_text(btn_l, "Snow");
    lv_obj_align(btn, img[2], LV_ALIGN_OUT_RIGHT_MID, -25, 0);

    /*************** minu plus temperature btn ****************/
    /*Create a style and use the new font*/
    static lv_style_t style1, style2, temp_style;
    lv_style_copy(&style1, &style_btn_min_plus_rel);
    style1.text.font = &lv_font_dejavu_40;
    style1.image.color = LV_COLOR_WHITE;

    lv_style_copy(&temp_style, &style_btn_min_plus_rel);
    temp_style.text.font = &lv_font_dejavu_40;
    temp_style.text.color = LV_COLOR_BLACK;

    lv_style_copy(&style2, &style_btn_pr);
    style2.text.font = &lv_font_dejavu_40;
    style2.image.color = LV_COLOR_BLACK;

    temperature = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(temperature, "21.5");
    lv_label_set_style(temperature, &temp_style);
    lv_obj_align(temperature, model_page, LV_ALIGN_OUT_RIGHT_MID, 38, 0);

    btn = lv_btn_create(lv_scr_act(), NULL);
    lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style1);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style2);
    lv_obj_set_size(btn, 50, 50);
    lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, plus_click);
    img[2] = lv_img_create(btn, NULL);
    lv_img_set_src(img[2], SYMBOL_PLUS);
    lv_obj_align(btn, temperature, LV_ALIGN_OUT_TOP_MID, 0, 5);
    lv_obj_animate(btn, LV_ANIM_FLOAT_RIGHT | LV_ANIM_IN, 200, 0, NULL);

    btn = lv_btn_create(lv_scr_act(), NULL);
    lv_btn_set_style(btn, LV_BTN_STYLE_REL, &style1);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, &style2);
    lv_obj_set_size(btn, 50, 50);
    lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, minus_click);
    img[2] = lv_img_create(btn, NULL);
    lv_img_set_src(img[2], SYMBOL_MINUS);
    lv_obj_align(btn, temperature, LV_ALIGN_OUT_BOTTOM_MID, 0, -10);
    lv_obj_animate(btn, LV_ANIM_FLOAT_RIGHT | LV_ANIM_IN, 200, 0, NULL);

    // temperature label animation after btn avoid animation invalid
    lv_obj_animate(temperature, LV_ANIM_FLOAT_RIGHT | LV_ANIM_IN, 200, 0, NULL);

    /*************** slide ****************/
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
    style_indic.body.grad_color = LV_COLOR_MAROON;
    style_indic.body.main_color = LV_COLOR_RED;
    style_indic.body.radius = LV_RADIUS_CIRCLE;
    style_indic.body.shadow.width = LV_DPI / 20;
    style_indic.body.shadow.color = LV_COLOR_RED;
    style_indic.body.padding.hor = LV_DPI / 30;
    style_indic.body.padding.ver = LV_DPI / 30;

    lv_style_copy(&style_knob, &lv_style_pretty);
    style_knob.body.radius = LV_RADIUS_CIRCLE;
    style_knob.body.opa = LV_OPA_70;

    /*Create a second slider*/
    slider = lv_slider_create(lv_scr_act(), NULL);
    lv_slider_set_style(slider, LV_SLIDER_STYLE_BG, &style_bar);
    lv_slider_set_style(slider, LV_SLIDER_STYLE_INDIC, &style_indic);
    lv_slider_set_style(slider, LV_SLIDER_STYLE_KNOB, &style_knob);
    lv_obj_set_size(slider, LV_DPI / 5, LV_VER_RES - 50);
    lv_obj_align(slider, holder_page, LV_ALIGN_IN_RIGHT_MID, -10, 0);

    lv_slider_set_action(slider, slider_action);
    lv_slider_set_range(slider, 16, 28);
    lv_slider_set_value(slider, 22);
    lv_obj_animate(slider, LV_ANIM_FLOAT_RIGHT | LV_ANIM_IN, 400, 0, NULL);
    lv_obj_animate(model_page, LV_ANIM_FLOAT_LEFT | LV_ANIM_IN, 400, 0, NULL);
}

/**
 * Called when the "Day" button is clicked
 * @param btn pointer to the button
 * @return LV_RES_OK because the button is not deleted in the function
 */
static lv_res_t day_click(lv_obj_t *btn)
{
    lv_img_set_src(wp, &lv_day);

    return LV_RES_OK;
}

static lv_res_t night_click(lv_obj_t *btn)
{
    lv_img_set_src(wp, &lv_night);

    return LV_RES_OK;
}

static lv_res_t vacation_click(lv_obj_t *btn)
{
    lv_img_set_src(wp, &lv_snow);

    return LV_RES_OK;
}

static lv_res_t minus_click(lv_obj_t *btn)
{
    char temp[10];
    temperature_num -= 0.5;
    if (temperature_num < 16) {
        temperature_num = 16;
    }
    sprintf(temp, "%2.1f", temperature_num);
    lv_label_set_text(temperature, temp);

    lv_slider_set_value(slider, (int16_t)temperature_num);
    return LV_RES_OK;
}

static lv_res_t plus_click(lv_obj_t *btn)
{
    char temp[10];
    temperature_num += 0.5;
    if (temperature_num > 28) {
        temperature_num = 28;
    }
    sprintf(temp, "%2.1f", temperature_num);
    lv_label_set_text(temperature, temp);

    lv_slider_set_value(slider, (int16_t)temperature_num);
    return LV_RES_OK;
}

static lv_res_t slider_action(lv_obj_t *slider)
{
    char temp[10];
    temperature_num = lv_slider_get_value(slider);
    sprintf(temp, "%2.1f", temperature_num);
    lv_label_set_text(temperature, temp);

    return LV_RES_OK;
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

    // thermostat initialize
    littlevgl_thermostat();

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
