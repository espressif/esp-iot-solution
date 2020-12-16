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


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "board.h"
#include "lvgl_gui.h"

#define TAG "thermostat"


/**********************
 *  STATIC VARIABLES
 **********************/
/* LVGL Object */
static lv_obj_t *holder_page;
static lv_obj_t *wp;
static lv_obj_t *temperature;
static lv_obj_t *slider;

static float temperature_num = 21.5;

/**********************
 *  IMAGE DECLARE
 **********************/
LV_IMG_DECLARE(lv_day);
LV_IMG_DECLARE(lv_night);
LV_IMG_DECLARE(lv_snow);



static void day_click(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        lv_img_set_src(wp, &lv_day);
    }
}

static void night_click(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        lv_img_set_src(wp, &lv_night);
    }

}

static void snow_click(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        lv_img_set_src(wp, &lv_snow);
    }

}

static void minus_click(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        char temp[10];
        temperature_num -= 0.5;
        if (temperature_num < 16) {
            temperature_num = 16;
        }
        sprintf(temp, "%2.1f", temperature_num);
        lv_label_set_text(temperature, temp);

        lv_slider_set_value(slider, (int16_t)temperature_num, true);
    }
}

static void plus_click(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        char temp[10];
        temperature_num += 0.5;
        if (temperature_num > 28) {
            temperature_num = 28;
        }
        sprintf(temp, "%2.1f", temperature_num);
        lv_label_set_text(temperature, temp);

        lv_slider_set_value(slider, (int16_t)temperature_num, true);
    }
}

static void slider_action(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        char temp[10];
        temperature_num = lv_slider_get_value(slider);
        sprintf(temp, "%2.1f", temperature_num);
        lv_label_set_text(temperature, temp);

    }
}

static void littlevgl_thermostat(void)
{
    /*Create a holder page (the page become scrollable on small displays )*/
    holder_page = lv_scr_act();

    /*Create a wallpaper on the page*/
    wp = lv_img_create(holder_page, NULL);
    lv_obj_add_protect(wp, LV_PROTECT_PARENT); /*Don't let to move the wallpaper by the layout */
    lv_img_set_src(wp, &lv_day);
    lv_obj_align(wp, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    /************* buttons **************/
    static lv_style_t style_btn_rel;
    lv_style_init(&style_btn_rel);
    lv_style_set_border_opa(&style_btn_rel, LV_STATE_DEFAULT, LV_OPA_50);
    lv_style_set_bg_opa(&style_btn_rel, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_style_set_text_color(&style_btn_rel, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x8b, 0x89, 0x89));
    /*************** Day ****************/
    lv_obj_t *btn_day = lv_btn_create(holder_page, NULL);
    lv_obj_set_size(btn_day, LV_DPX(100), LV_DPX(50));
    lv_obj_add_style(btn_day, LV_OBJ_PART_MAIN, &style_btn_rel);
    lv_obj_set_event_cb(btn_day, day_click);
    lv_obj_t *label_day = lv_label_create(btn_day, NULL);
    lv_label_set_text(label_day, "Day");
    lv_obj_align(btn_day, NULL, LV_ALIGN_IN_LEFT_MID, 10, -70);

    /*************** Night ****************/
    lv_obj_t *btn_night;
    btn_night = lv_btn_create(holder_page, NULL);
    lv_obj_set_size(btn_night, LV_DPX(100), LV_DPX(50));
    lv_obj_add_style(btn_night,LV_OBJ_PART_MAIN, &style_btn_rel);
    lv_obj_set_event_cb(btn_night, night_click);
    lv_obj_t *label_night = lv_label_create(btn_night, NULL);
    lv_label_set_text(label_night, "Night");
    lv_obj_align(btn_night, NULL, LV_ALIGN_IN_LEFT_MID, 10, 0);

    /*************** Snow ****************/
    lv_obj_t *btn_snow;
    btn_snow = lv_btn_create(holder_page, NULL);
    lv_obj_set_size(btn_snow, LV_DPX(100), LV_DPX(50));
    lv_obj_add_style(btn_snow, LV_OBJ_PART_MAIN,&style_btn_rel);
    lv_obj_set_event_cb(btn_snow, snow_click);
    lv_obj_t *label_snow = lv_label_create(btn_snow, NULL);
    lv_label_set_text(label_snow, "Snow");
    lv_obj_align(btn_snow, NULL, LV_ALIGN_IN_LEFT_MID, 10, 70);

    /*************** minu plus temperature btn ****************/
    /*Create a style and use the new font*/
    static lv_style_t temp_style;
    lv_style_init(&temp_style);
    lv_style_set_text_font(&temp_style, LV_STATE_DEFAULT, &lv_font_montserrat_32);
    lv_style_set_text_color(&temp_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    temperature = lv_label_create(holder_page, NULL);
    lv_label_set_text(temperature, "21.5");
    lv_obj_add_style(temperature, LV_OBJ_PART_MAIN, &temp_style);
    lv_obj_align(temperature, NULL, LV_ALIGN_IN_RIGHT_MID, -112, 0);

    static lv_style_t temp_btn_style;
    lv_style_init(&temp_btn_style);
    lv_style_set_bg_opa(&temp_btn_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_style_set_border_opa(&temp_btn_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_style_set_border_opa(&temp_btn_style, LV_STATE_FOCUSED, LV_OPA_TRANSP);

    lv_obj_t *btn_add = lv_btn_create(holder_page, NULL);
    lv_obj_add_style(btn_add, LV_OBJ_PART_MAIN, &temp_btn_style);
    lv_obj_set_size(btn_add, 50, 50);
    lv_obj_set_event_cb(btn_add, plus_click);
    lv_obj_t *img_add = lv_img_create(btn_add, NULL);
    lv_img_set_src(img_add, LV_SYMBOL_PLUS);
    lv_obj_align(btn_add, temperature, LV_ALIGN_OUT_TOP_MID, 0, 5);

    lv_obj_t *btn_sub = lv_btn_create(holder_page, NULL);
    lv_obj_add_style(btn_sub, LV_OBJ_PART_MAIN, &temp_btn_style);
    lv_obj_set_size(btn_sub, 50, 50);
    lv_obj_set_event_cb(btn_sub, minus_click);
    lv_obj_t *img_sub = lv_img_create(btn_sub, NULL);
    lv_img_set_src(img_sub, LV_SYMBOL_MINUS);
    lv_obj_align(btn_sub, temperature, LV_ALIGN_OUT_BOTTOM_MID, 0, -10);

    // /*************** slide ****************/
    static lv_style_t style_bar;
    static lv_style_t style_indic;
    static lv_style_t style_knob;

    lv_style_init(&style_bar);
    lv_style_set_bg_color(&style_bar, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_bg_grad_color(&style_bar, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_style_set_radius(&style_bar, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_style_set_border_color(&style_bar, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_bg_opa(&style_bar, LV_STATE_DEFAULT, LV_OPA_60);

    lv_style_init(&style_indic);
    lv_style_set_bg_color(&style_indic, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_style_set_radius(&style_indic, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_style_set_shadow_width(&style_indic, LV_STATE_DEFAULT, LV_DPI / 20);
    lv_style_set_shadow_color(&style_indic, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_style_set_bg_opa(&style_indic, LV_STATE_DEFAULT, LV_OPA_60);

    lv_style_init(&style_knob);
    lv_style_set_bg_opa(&style_knob, LV_STATE_DEFAULT, LV_OPA_80);

    /*Create a second slider*/
    slider = lv_slider_create(holder_page, NULL);
    lv_obj_add_style(slider, LV_SLIDER_PART_BG, &style_bar);
    lv_obj_add_style(slider, LV_SLIDER_PART_INDIC, &style_indic);
    lv_obj_add_style(slider, LV_SLIDER_PART_KNOB, &style_knob);
    lv_obj_set_size(slider, 15, LV_VER_RES - 50);
    lv_obj_align(slider, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);

    lv_obj_set_event_cb(slider, slider_action);
    lv_slider_set_range(slider, 16, 28);
    lv_slider_set_anim_time(slider, 0);
    lv_slider_set_value(slider, 22, true);
}


/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
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

    // thermostat initialize
    littlevgl_thermostat();

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
