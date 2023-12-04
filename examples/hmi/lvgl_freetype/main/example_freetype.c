/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_check.h"
#include "esp_spiffs.h"

#include "lv_demos.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "freetype";

LV_FONT_DECLARE(font_Kaiti30)

#define FREAM_SIZE      4*1024
#define FONT_PATH       "/spiffs/KaiTi.ttf"

/* Print log about SRAM and PSRAM memory */
#define LOG_MEM_INFO    (0)

static lv_obj_t *label_map_info = NULL;
static lv_obj_t *label_info = NULL;
static lv_obj_t *label_reference = NULL;

esp_err_t freetype_ttf_preload(lv_ft_info_t *info)
{
    esp_err_t ret = ESP_OK;

    uint32_t font_file_size, total_read;
    uint8_t *font_stream_mem = NULL;
    FILE  *file = NULL;

    if ((file = fopen(FONT_PATH, "rb")) == NULL) {
        ESP_LOGE(TAG, "%s open failed", FONT_PATH);
        return ESP_FAIL;
    }

    fseek(file, 0, SEEK_END);
    font_file_size = (unsigned long)ftell(file);
    ESP_GOTO_ON_FALSE(font_file_size, ESP_ERR_INVALID_ARG, err, TAG, "font zero-sized");

    font_stream_mem = (uint8_t *)heap_caps_malloc(font_file_size + 2, MALLOC_CAP_SPIRAM);
    ESP_GOTO_ON_FALSE(font_stream_mem, ESP_ERR_NO_MEM, err, TAG, "mem malloc failed");

    total_read = 0;
    fseek(file, 0, SEEK_SET);
    do {
        uint32_t len = 0;
        do {
            len += fread(font_stream_mem + total_read + len, 1, (FREAM_SIZE - len), file);
            if (1 == feof(file)) {
                break;
            }
        } while (len < FREAM_SIZE);

        total_read += len;
    } while (0 == feof(file));
    info->mem = font_stream_mem;
    info->mem_size = font_file_size;
    ESP_LOGI(TAG, "ttf load ok,[%d Kb, %d Kb]", total_read / 1024, font_file_size / 1024);

    return ESP_OK;
err:
    fclose(file);
    if (font_stream_mem) {
        heap_caps_free(font_stream_mem);
    }
    return ret;
}

static void press_event_cb(lv_event_t *e)
{
    static uint8_t presstest = 0;
    static char buttonCount[100];

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        presstest++;
        ESP_LOGI(TAG, "press_event_cb start");

        memset(buttonCount, 0, sizeof(buttonCount));
        sprintf(buttonCount, "按键次数:%d", presstest);

        lv_label_set_text(label_map_info, buttonCount);
        lv_label_set_text(label_info, buttonCount);
        lv_label_set_text(label_reference, buttonCount);
    }
}

bool create_example_freetype(void)
{
    bool bRet = false;
    static lv_ft_info_t info40 = {
        .weight = 40,
        .style = FT_FONT_STYLE_NORMAL,
        .name = FONT_PATH,
        .mem = NULL,
    };

    static lv_ft_info_t info30 = {
        .weight = 30,
        .style = FT_FONT_STYLE_NORMAL,
        .name = FONT_PATH,
        .mem = NULL,
    };

    ESP_ERROR_CHECK(freetype_ttf_preload(&info30));
    info40.mem = info30.mem;
    info40.mem_size = info30.mem_size;

    bRet = lv_ft_font_init(&info30);
    ESP_ERROR_CHECK(((true == bRet) ? ESP_OK : ESP_FAIL));

    bRet = lv_ft_font_init(&info40);
    ESP_ERROR_CHECK(((true == bRet) ? ESP_OK : ESP_FAIL));

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(54, 79, 107), LV_PART_MAIN);

    lv_obj_t *btn_press = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn_press, press_event_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_align(btn_press, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_press, lv_color_make(63, 193, 201), LV_PART_MAIN);
    lv_obj_set_size(btn_press, 160, 70);

    lv_obj_t *label_press = lv_label_create(btn_press);
    lv_obj_set_style_text_font(label_press, info30.font, 0);
    lv_label_set_text(label_press, "按键");
    lv_obj_center(label_press);

    label_map_info = lv_label_create(lv_scr_act());
    label_info = lv_label_create(lv_scr_act());
    label_reference = lv_label_create(lv_scr_act());

    lv_obj_set_style_text_color(label_map_info, lv_color_make(246, 246, 246), 0);
    lv_obj_set_style_text_color(label_info, lv_color_make(246, 246, 246), 0);
    lv_obj_set_style_text_color(label_reference, lv_color_make(246, 246, 246), 0);

    lv_obj_set_style_text_font(label_map_info, &font_Kaiti30, 0);
    lv_obj_set_style_text_font(label_info, info30.font, 0);
    lv_obj_set_style_text_font(label_reference, info40.font, 0);

    lv_obj_align(label_map_info, LV_ALIGN_CENTER, 0, -100);
    lv_obj_align(label_info, LV_ALIGN_CENTER, 0, -50);
    lv_obj_align(label_reference, LV_ALIGN_CENTER, 0, 50);

    lv_label_set_text(label_map_info, "Map:info30 中文显示测试");
    lv_label_set_text(label_info, "Font:info30 中文显示测试");
    lv_label_set_text(label_reference, "Font:info40 中文显示测试");

    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();

    /* Initialize display and LVGL */
    bsp_display_start();

    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    /* Mount SPIFFS */
    bsp_spiffs_mount();

    bsp_display_lock(0);
    create_example_freetype();
    bsp_display_unlock();

#if LOG_MEM_INFO
    static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
    while (1) {
        /**
         * It's not recommended to frequently use functions like `heap_caps_get_free_size()` to obtain memory information
         * in practical applications, especially when the application extensively uses `malloc()` to dynamically allocate
         * a significant number of memory blocks. The frequent interrupt disabling may potentially lead to issues with other functionalities.
         */
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                "\t  SRAM : [%8d / %8d / %8d]\n"
                "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI("MEM", "%s", buffer);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#endif
}
