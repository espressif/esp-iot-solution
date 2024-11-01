/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "mmap_generate_spiffs_assets.h"

static const char *TAG = "freetype";

static mmap_assets_handle_t asset_handle;

static lv_obj_t *label_vector_font30 = NULL;
static lv_obj_t *label_vector_font40 = NULL;

static void image_mmap_init(void)
{
    const mmap_assets_config_t config = {
        .partition_label = "assets",
        .max_files = MMAP_SPIFFS_ASSETS_FILES,
        .checksum = MMAP_SPIFFS_ASSETS_CHECKSUM,
        .flags = {.mmap_enable = true}
    };

    ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));
}

static void press_event_cb(lv_event_t *e)
{
    static int presstest = 0;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        presstest++;
        lv_label_set_text_fmt(label_vector_font30, "PRESS %d", presstest);
        lv_label_set_text_fmt(label_vector_font40, "PRESS %d", presstest);
    }
}

void freetype_task(void *arg)
{
    bool bRet = false;
    static lv_ft_info_t font40 = {
        .weight = 40,
        .style = FT_FONT_STYLE_NORMAL,
        .name = "DejaVuSans.ttf",
        .mem = NULL,
    };

    static lv_ft_info_t font30 = {
        .weight = 30,
        .style = FT_FONT_STYLE_NORMAL,
        .name = "DejaVuSans.ttf",
        .mem = NULL,
    };

    font30.mem = mmap_assets_get_mem(asset_handle, MMAP_SPIFFS_ASSETS_DEJAVUSANS_TTF);
    font30.mem_size = mmap_assets_get_size(asset_handle, MMAP_SPIFFS_ASSETS_DEJAVUSANS_TTF);

    font40.mem = mmap_assets_get_mem(asset_handle, MMAP_SPIFFS_ASSETS_DEJAVUSANS_TTF);
    font40.mem_size = mmap_assets_get_size(asset_handle, MMAP_SPIFFS_ASSETS_DEJAVUSANS_TTF);

    bsp_display_lock(0);

    bRet = lv_ft_font_init(&font30);
    ESP_ERROR_CHECK(((true == bRet) ? ESP_OK : ESP_FAIL));

    bRet = lv_ft_font_init(&font40);
    ESP_ERROR_CHECK(((true == bRet) ? ESP_OK : ESP_FAIL));

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(54, 79, 107), LV_PART_MAIN);

    lv_obj_t *btn_press = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn_press, press_event_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_align(btn_press, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_press, lv_color_make(63, 193, 201), LV_PART_MAIN);
    lv_obj_set_size(btn_press, 160, 70);

    lv_obj_t *label_press = lv_label_create(btn_press);
    lv_obj_set_style_text_font(label_press, font30.font, 0);
    lv_label_set_text(label_press, "Press me");
    lv_obj_center(label_press);

    lv_obj_t *label_font_name = lv_label_create(lv_scr_act());
    label_vector_font30 = lv_label_create(lv_scr_act());
    label_vector_font40 = lv_label_create(lv_scr_act());

    lv_obj_set_style_text_color(label_font_name, lv_color_make(246, 246, 246), 0);
    lv_obj_set_style_text_color(label_vector_font30, lv_color_make(246, 246, 246), 0);
    lv_obj_set_style_text_color(label_vector_font40, lv_color_make(246, 246, 246), 0);

    lv_obj_set_style_text_font(label_font_name, font30.font, 0);
    lv_obj_set_style_text_font(label_vector_font30, font30.font, 0);
    lv_obj_set_style_text_font(label_vector_font40, font40.font, 0);

    lv_obj_align(label_font_name, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align(label_vector_font30, LV_ALIGN_CENTER, 0, -50);
    lv_obj_align(label_vector_font40, LV_ALIGN_CENTER, 0, 50);

    lv_label_set_text(label_vector_font30, "Sample Text: Size 30");
    lv_label_set_text(label_vector_font40, "Sample Text: Size 40");
    lv_label_set_text_fmt(label_font_name, "%s", font30.name);

    bsp_display_unlock();

    vTaskDelete(NULL);
}

void app_main(void)
{
    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();

    /* Initialize display and LVGL */
    bsp_display_start();

    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    /* Mount SPIFFS */
    image_mmap_init();

    BaseType_t res = xTaskCreate(freetype_task, "freetype task", 30 * 1024, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Create freetype task fail!");
    }
}
