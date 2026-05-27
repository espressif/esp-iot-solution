/* SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_painter.h"
#include "esp_io_expander_tca9554.h"
#include "esp_jpeg_dec.h"
#include "esp_timer.h"
#include "iot_button.h"
#include "app_uvc.h"

static const char *TAG = "uvc_camera_lcd_demo";
/****************** configure the example working mode *******************************/

#define DEMO_SWITCH_BUTTON_IO                0
#define DEMO_UVC_MAX_WIDTH                   1920
#define DEMO_UVC_MAX_HEIGHT                  1080

static app_uvc_frame_size_t camera_frame_size           = {0};
static esp_painter_handle_t painter                     = NULL;
static esp_lcd_panel_handle_t panel_handle              = NULL;
static uint8_t *rgb_frame_buf1                          = NULL;
static uint8_t *rgb_frame_buf2                          = NULL;
static uint8_t *cur_frame_buf                           = NULL;
static uint8_t *decoded_frame_buf                       = NULL;
static size_t decoded_frame_buf_size                    = 0;
static uint16_t current_width                           = 0;
static uint16_t current_height                          = 0;
static bool current_frame_size_supported                 = false;
static uint16_t display_width                           = 0;
static uint16_t display_height                          = 0;
static bool display_need_copy                           = false;
static int display_fps                                  = 0;

static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    // Generate default configuration.
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    if (current_width != display_width || current_height != display_height) {
        config.scale.width = display_width;
        config.scale.height = display_height;
    }

    // Empty handle to jpeg_decoder.
    jpeg_dec_handle_t jpeg_dec = NULL;
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG decoder open failed: %d", ret);
        return ESP_FAIL;
    }

    // Create io_callback handle.
    jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        ESP_LOGE(TAG, "No memory for JPEG IO");
        jpeg_dec_close(jpeg_dec);
        return ESP_FAIL;
    }

    // Create out_info handle.
    jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        ESP_LOGE(TAG, "No memory for JPEG header info");
        free(jpeg_io);
        jpeg_dec_close(jpeg_dec);
        return ESP_FAIL;
    }

    // Set input buffer and buffer len to io_callback.
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder.
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG parse header failed: %d", ret);
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;

    // Start decode jpeg raw data.
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %d", ret);
    }

_exit:
    // Decoder deinitialize.
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return ret == JPEG_ERR_OK ? ESP_OK : ESP_FAIL;
}

static bool ensure_decoded_frame_buffer(size_t required_size)
{
    if (decoded_frame_buf != NULL && decoded_frame_buf_size >= required_size) {
        return true;
    }

    if (decoded_frame_buf != NULL) {
        free(decoded_frame_buf);
        decoded_frame_buf = NULL;
        decoded_frame_buf_size = 0;
    }

    decoded_frame_buf = (uint8_t *)heap_caps_aligned_alloc(16, required_size, MALLOC_CAP_SPIRAM);
    if (decoded_frame_buf == NULL) {
        ESP_LOGE(TAG, "No memory for decoded frame buffer: %u bytes", (unsigned int)required_size);
        return false;
    }
    decoded_frame_buf_size = required_size;
    return true;
}

static void copy_decoded_frame_to_back_buffer(uint16_t *frame_buffer, const uint16_t *decoded_buffer)
{
    int x_start = (BSP_LCD_H_RES - display_width) / 2;
    int y_start = (BSP_LCD_V_RES - display_height) / 2;

    // Copy one compact RGB565 frame into the back buffer with LCD stride.
    for (int y = 0; y < display_height; y++) {
        memcpy(&frame_buffer[(y_start + y) * BSP_LCD_H_RES + x_start], &decoded_buffer[y * display_width], display_width * sizeof(uint16_t));
    }
}

static uint16_t align_down_to_8(uint16_t value)
{
    return value & ~0x7;
}

static void update_frame_size(uint16_t frame_width, uint16_t frame_height)
{
    current_width = frame_width;
    current_height = frame_height;
    display_width = current_width;
    display_height = current_height;
    current_frame_size_supported = current_width > 0 && current_height > 0;

    if (current_frame_size_supported && (current_width > BSP_LCD_H_RES || current_height > BSP_LCD_V_RES)) {
        uint32_t width_by_height = (uint32_t)current_width * BSP_LCD_V_RES / current_height;
        if (width_by_height <= BSP_LCD_H_RES) {
            display_width = align_down_to_8(width_by_height);
            display_height = align_down_to_8(BSP_LCD_V_RES);
        } else {
            display_width = align_down_to_8(BSP_LCD_H_RES);
            display_height = align_down_to_8((uint32_t)current_height * BSP_LCD_H_RES / current_width);
        }
        current_frame_size_supported = display_width > 0 && display_height > 0 && display_width <= current_width && display_height <= current_height;
    }

    display_need_copy = display_width != BSP_LCD_H_RES;
    memset(rgb_frame_buf1, 0, BSP_LCD_H_RES * BSP_LCD_V_RES * 2);
    memset(rgb_frame_buf2, 0, BSP_LCD_H_RES * BSP_LCD_V_RES * 2);
    if (!current_frame_size_supported) {
        ESP_LOGE(TAG, "Unsupported camera frame size: %d*%d", current_width, current_height);
        return;
    }

    if (display_need_copy && !ensure_decoded_frame_buffer(display_width * display_height * sizeof(uint16_t))) {
        current_frame_size_supported = false;
        return;
    }
    ESP_LOGI(TAG, "Display camera frame %d*%d as %d*%d", current_width, current_height, display_width, display_height);
}

static void camera_frame_cb(const uvc_host_frame_t *frame, void *user_ctx)
{
    uint16_t frame_width = frame->vs_format.h_res;
    uint16_t frame_height = frame->vs_format.v_res;
    static int64_t last_frame_time = 0;
    int y_start = 0;
    uint8_t *decode_buffer = NULL;

    if (current_width != frame_width || current_height != frame_height) {
        update_frame_size(frame_width, frame_height);
    }

    if (!current_frame_size_supported) {
        return;
    }

    if (display_need_copy) {
        decode_buffer = decoded_frame_buf;
    } else {
        y_start = (BSP_LCD_V_RES - display_height) / 2;
        decode_buffer = cur_frame_buf + y_start * BSP_LCD_H_RES * sizeof(uint16_t);
    }

    if (esp_jpeg_decoder_one_picture(frame->data, frame->data_len, decode_buffer) != ESP_OK) {
        ESP_LOGE(TAG, "Decode camera frame failed");
        return;
    }

    if (display_need_copy) {
        copy_decoded_frame_to_back_buffer((uint16_t *)cur_frame_buf, (uint16_t *)decoded_frame_buf);
    }

    // Draw transparent text inside the refreshed image area so the next frame clears old glyphs.
    int overlay_x = display_need_copy ? (BSP_LCD_H_RES - display_width) / 2 : 0;
    int overlay_y = (BSP_LCD_V_RES - display_height) / 2;
    ESP_ERROR_CHECK(esp_painter_set_buffer(painter, cur_frame_buf, BSP_LCD_H_RES));
    if (current_width != display_width || current_height != display_height) {
        ESP_ERROR_CHECK(esp_painter_draw_string_format(
                            painter, overlay_x, overlay_y, NULL, COLOR_RGB565_RED, "FPS:%d %dx%d>%dx%d",
                            display_fps, current_width, current_height, display_width, display_height
                        ));
    } else {
        ESP_ERROR_CHECK(esp_painter_draw_string_format(
                            painter, overlay_x, overlay_y, NULL, COLOR_RGB565_RED, "FPS:%d %dx%d", display_fps, display_width, display_height
                        ));
    }
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, cur_frame_buf);
    cur_frame_buf = (cur_frame_buf == rgb_frame_buf1) ? rgb_frame_buf2 : rgb_frame_buf1;

    int64_t current_frame_time = esp_timer_get_time();
    if (last_frame_time != 0) {
        int64_t frame_interval = current_frame_time - last_frame_time;
        if (frame_interval > 0) {
            display_fps = 1000000 / frame_interval;
            ESP_LOGI(TAG, "camera fps: %d %d*%d", display_fps, current_width, current_height);
        }
    }
    last_frame_time = current_frame_time;
    vTaskDelay(pdMS_TO_TICKS(1));
}

static esp_err_t _display_init(void)
{
    bsp_i2c_init();
    bsp_display_config_t disp_config = {0};
    bsp_display_new(&disp_config, &panel_handle, NULL);
    assert(panel_handle);
    esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, (void **)&rgb_frame_buf1, (void **)&rgb_frame_buf2);
    cur_frame_buf = rgb_frame_buf2;

    esp_painter_config_t painter_config = {
        .brush.color = COLOR_RGB565_RED,
        .canvas = {
            .x = 0,
            .y = 0,
            .width = BSP_LCD_H_RES,
            .height = BSP_LCD_V_RES,
            .color = COLOR_CANVAS_TRANS_BG,
            .buffer = cur_frame_buf,
            .stride = BSP_LCD_H_RES,
        },
        .default_font = &esp_painter_basic_font_24,
        .piexl_color_byte = 2,
        .lcd_panel = NULL,
    };
    ESP_ERROR_CHECK(esp_painter_new(&painter_config, &painter));
    return ESP_OK;
}

static void switch_button_press_down_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "old resolution is %d*%d", camera_frame_size.width, camera_frame_size.height);
    esp_err_t err = app_uvc_switch_resolution(&camera_frame_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Switch UVC resolution failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "current resolution is %d*%d", camera_frame_size.width, camera_frame_size.height);
    esp_lcd_rgb_panel_restart(panel_handle);
}

static esp_err_t _switch_button_init(void)
{
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = DEMO_SWITCH_BUTTON_IO,
            .active_level = 0,
        },
    };

    button_handle_t button_handle = iot_button_create(&button_config);
    assert(button_handle != NULL);
    esp_err_t ret = iot_button_register_cb(button_handle, BUTTON_PRESS_DOWN, switch_button_press_down_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "button register callback fail: %s", esp_err_to_name(ret));
    }
    return ret;
}

void app_main(void)
{
    /* Initialize the screen */
    ESP_ERROR_CHECK(_display_init());

    /* Initialize the button to switch resolution */
    ESP_ERROR_CHECK(_switch_button_init());

    /* Initialize USB Host UVC and wait for camera hotplug events */
    app_uvc_config_t uvc_config = {
        .max_width = DEMO_UVC_MAX_WIDTH,
        .max_height = DEMO_UVC_MAX_HEIGHT,
        .frame_cb = camera_frame_cb,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(app_uvc_init(&uvc_config));

    /* Start with the LCD-sized camera resolution first if the camera exposes one. */
    const app_uvc_frame_size_t preferred_frame_size = {
        .width = BSP_LCD_H_RES,
        .height = BSP_LCD_V_RES,
    };
    ESP_ERROR_CHECK(app_uvc_start(&preferred_frame_size, &camera_frame_size));
}
