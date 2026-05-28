/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "app_lcd.h"
#include "app_usb.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "driver/jpeg_decode.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "app_lcd";

static esp_lcd_panel_handle_t display_handle;
static jpeg_decoder_handle_t jpgd_handle;
static uint32_t out_size;
static void *lcd_buffer[EXAMPLE_LCD_BUF_NUM];
static uint8_t buf_index;

static const jpeg_decode_cfg_t decode_cfg = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
};

void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height)
{
    static int fps_count;
    static int64_t start_time;

    if (width != EXAMPLE_LCD_H_RES || height != EXAMPLE_LCD_V_RES) {
        ESP_LOGW(TAG, "Drop JPEG with unexpected size: %ux%u, expected %ux%u",
                 width, height, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
        return;
    }

    fps_count++;
    if (fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;
    }

    esp_err_t ret = jpeg_decoder_process(jpgd_handle, &decode_cfg, buf, len,
                                         lcd_buffer[buf_index], EXAMPLE_LCD_BUF_LEN, &out_size);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_lcd_panel_draw_bitmap(display_handle, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, lcd_buffer[buf_index]);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "LCD draw failed: %s", esp_err_to_name(ret));
        return;
    }

    buf_index = (buf_index + 1) == EXAMPLE_LCD_BUF_NUM ? 0 : (buf_index + 1);
}

esp_err_t app_lcd_init(void)
{
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .intr_priority = 1,
        .timeout_ms = 50,
    };

    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle), TAG, "create JPEG decoder failed");

    bsp_display_config_t disp_config = {
        .dpi_fb_buf_num = EXAMPLE_LCD_BUF_NUM,
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&disp_config, &display_handle, NULL), TAG, "create display failed");
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "brightness init failed");
    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "backlight on failed");

#if EXAMPLE_LCD_BUF_NUM == 1
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 1, &lcd_buffer[0]), TAG, "get LCD frame buffer failed");
#elif EXAMPLE_LCD_BUF_NUM == 2
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 2, &lcd_buffer[0], &lcd_buffer[1]), TAG, "get LCD frame buffer failed");
#else
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 3, &lcd_buffer[0], &lcd_buffer[1], &lcd_buffer[2]), TAG, "get LCD frame buffer failed");
#endif

    return ESP_OK;
}
