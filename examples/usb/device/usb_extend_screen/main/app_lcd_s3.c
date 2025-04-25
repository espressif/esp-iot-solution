/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_types.h"
#include "app_lcd.h"
#include "app_usb.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_timer.h"
#include "esp_jpeg_dec.h"

static const char *TAG = "app_lcd";

static esp_lcd_panel_handle_t display_handle;

static void *lcd_buffer[CONFIG_BSP_LCD_RGB_BUFFER_NUMS];
static uint8_t buf_index = 0;

static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    esp_err_t ret = ESP_OK;
    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        return ESP_FAIL;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        return ESP_FAIL;
    }
    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0) {
        goto _exit;
    }

_exit:
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return ret;
}

void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height)
{
    static int fps_count = 0;
    static int64_t start_time = 0;
    fps_count++;
    if (fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;
    }

    esp_jpeg_decoder_one_picture((uint8_t *)buf, len, lcd_buffer[buf_index]);

    esp_lcd_panel_draw_bitmap(display_handle, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, lcd_buffer[buf_index]);

    buf_index = (buf_index + 1) == CONFIG_BSP_LCD_RGB_BUFFER_NUMS ? 0 : (buf_index + 1);
}

esp_err_t app_lcd_init(void)
{
    bsp_display_config_t disp_config = {0};
    bsp_display_new(&disp_config, &display_handle, NULL);
    bsp_display_brightness_init();
    bsp_display_backlight_on();

#if CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 1
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 1, &lcd_buffer[0]));
#elif CONFIG_BSP_LCD_RGB_BUFFER_NUMS == 2
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 2, &lcd_buffer[0], &lcd_buffer[1]));
#else
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 3, &lcd_buffer[0], &lcd_buffer[1], &lcd_buffer[2]));
#endif
    return ESP_OK;
}
