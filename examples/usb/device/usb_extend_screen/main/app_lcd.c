/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "app_lcd.h"
#include "driver/ledc.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"
#else
#include "esp_jpeg_dec.h"
#endif

static const char *TAG = "app_lcd";

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#define LCD_DECODE_BUFFER_COUNT  CONFIG_EXAMPLE_LCD_BUF_COUNT
#define LCD_JPEG_MAX_MCU_ALIGNMENT  16U
#define LCD_ALIGN_UP(value, alignment)  (((value) + (alignment) - 1) & ~((alignment) - 1))
#else
#define LCD_DECODE_BUFFER_COUNT  1
#endif

static esp_lcd_panel_handle_t s_panel;
static void *s_decode_buffers[LCD_DECODE_BUFFER_COUNT];
static size_t s_decode_buffer_sizes[LCD_DECODE_BUFFER_COUNT];
static size_t s_decode_buffer_len;
static uint16_t s_lcd_width;
static uint16_t s_lcd_height;
static size_t s_lcd_bytes_per_pixel;
static dev_display_lcd_frame_format_t s_lcd_frame_format;
static uint8_t s_buffer_index;

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
static jpeg_decoder_handle_t s_jpeg_decoder;

static jpeg_decode_cfg_t s_decode_config = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
#if CONFIG_IDF_TARGET_ESP32S31
    .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
#endif
};
#endif

static size_t app_lcd_frame_format_bytes_per_pixel(dev_display_lcd_frame_format_t format)
{
    switch (format) {
    case DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_LE:
    case DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_BE:
        return 2;
    case DEV_DISPLAY_LCD_FRAME_FORMAT_BGR888:
    case DEV_DISPLAY_LCD_FRAME_FORMAT_RGB888:
        return 3;
    default:
        return 0;
    }
}

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
static bool app_lcd_frame_format_uses_rgb_order(dev_display_lcd_frame_format_t format)
{
    return format == DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_BE ||
           format == DEV_DISPLAY_LCD_FRAME_FORMAT_RGB888;
}
#endif

static const char *app_lcd_frame_format_name(dev_display_lcd_frame_format_t format)
{
    switch (format) {
    case DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_LE:
        return "RGB565_LE";
    case DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_BE:
        return "RGB565_BE";
    case DEV_DISPLAY_LCD_FRAME_FORMAT_BGR888:
        return "BGR888";
    case DEV_DISPLAY_LCD_FRAME_FORMAT_RGB888:
        return "RGB888";
    default:
        return "unknown";
    }
}

static esp_err_t app_lcd_backlight_set(uint32_t brightness_percent)
{
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS)) {
        return ESP_OK;
    }

    brightness_percent = MIN(brightness_percent, 100);
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS),
                        TAG, "initialize LCD brightness device failed");

    periph_ledc_handle_t *ledc_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS,
                                                            (void **)&ledc_handle),
                        TAG, "get LCD brightness handle failed");

    dev_ledc_ctrl_config_t *brightness_config = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS,
                                                            (void **)&brightness_config),
                        TAG, "get LCD brightness config failed");

    periph_ledc_config_t *ledc_config = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_periph_config(brightness_config->ledc_name,
                                                            (void **)&ledc_config),
                        TAG, "get LCD backlight LEDC config failed");

    uint32_t max_duty = (1U << ledc_config->duty_resolution) - 1;
    uint32_t duty = brightness_percent * max_duty / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(ledc_handle->speed_mode, ledc_handle->channel, duty),
                        TAG, "set LCD backlight duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(ledc_handle->speed_mode, ledc_handle->channel),
                        TAG, "update LCD backlight duty failed");
#else
    (void)brightness_percent;
#endif
    return ESP_OK;
}

static esp_err_t app_lcd_allocate_decode_buffers(void)
{
    size_t output_width = s_lcd_width;
    size_t output_height = s_lcd_height;
#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
    /*
     * The hardware decoder pads its output to the JPEG MCU size. YUV420 is the
     * worst case (16x16), so this capacity also covers YUV422 (16x8) and
     * YUV444 (8x8), including panels whose visible size is not MCU-aligned.
     */
    output_width = LCD_ALIGN_UP(output_width, LCD_JPEG_MAX_MCU_ALIGNMENT);
    output_height = LCD_ALIGN_UP(output_height, LCD_JPEG_MAX_MCU_ALIGNMENT);
    ESP_LOGI(TAG, "JPEG buffer: visible=%ux%u, MCU-aligned capacity=%ux%u",
             s_lcd_width, s_lcd_height, (unsigned)output_width, (unsigned)output_height);
#endif
    s_decode_buffer_len = output_width * output_height * s_lcd_bytes_per_pixel;

    for (size_t i = 0; i < LCD_DECODE_BUFFER_COUNT; i++) {
#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
        jpeg_decode_memory_alloc_cfg_t output_memory_config = {
            .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
        };
        s_decode_buffers[i] = jpeg_alloc_decoder_mem(s_decode_buffer_len, &output_memory_config,
                                                     &s_decode_buffer_sizes[i]);
#else
        s_decode_buffers[i] = jpeg_calloc_align(s_decode_buffer_len, 16);
        s_decode_buffer_sizes[i] = s_decode_buffer_len;
#endif
        ESP_RETURN_ON_FALSE(s_decode_buffers[i], ESP_ERR_NO_MEM, TAG,
                            "allocate LCD decode buffer %u failed", (unsigned)i);
    }
    return ESP_OK;
}

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
static esp_err_t app_lcd_get_jpeg_output_layout(const uint8_t *input, size_t input_len,
                                                uint16_t width, uint16_t height,
                                                uint32_t *output_width, uint32_t *output_height)
{
    jpeg_decode_picture_info_t picture_info = {0};
    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(input, input_len, &picture_info),
                        TAG, "parse JPEG header failed");
    ESP_RETURN_ON_FALSE(picture_info.width == width && picture_info.height == height,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "JPEG size is %ux%u, expected %ux%u",
                        (unsigned)picture_info.width, (unsigned)picture_info.height, width, height);

    uint32_t horizontal_alignment;
    uint32_t vertical_alignment;
    switch (picture_info.sample_method) {
    case JPEG_DOWN_SAMPLING_YUV444:
        horizontal_alignment = 8;
        vertical_alignment = 8;
        break;
    case JPEG_DOWN_SAMPLING_YUV422:
        horizontal_alignment = 16;
        vertical_alignment = 8;
        break;
    case JPEG_DOWN_SAMPLING_YUV420:
        horizontal_alignment = 16;
        vertical_alignment = 16;
        break;
    case JPEG_DOWN_SAMPLING_GRAY:
        return ESP_ERR_NOT_SUPPORTED;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    *output_width = LCD_ALIGN_UP(picture_info.width, horizontal_alignment);
    *output_height = LCD_ALIGN_UP(picture_info.height, vertical_alignment);
    return ESP_OK;
}
#endif

#if !CONFIG_SOC_JPEG_DECODE_SUPPORTED
static jpeg_error_t app_lcd_decode_jpeg(const uint8_t *input, size_t input_len, uint8_t *output,
                                        size_t output_len, uint16_t width, uint16_t height)
{
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    if (s_lcd_bytes_per_pixel == 3) {
        config.output_type = JPEG_PIXEL_FORMAT_RGB888;
    } else {
        config.output_type = s_lcd_frame_format == DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_BE ?
                             JPEG_PIXEL_FORMAT_RGB565_BE : JPEG_PIXEL_FORMAT_RGB565_LE;
    }

    jpeg_dec_handle_t decoder = NULL;
    jpeg_error_t ret = jpeg_dec_open(&config, &decoder);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    jpeg_dec_io_t io = {
        .inbuf = (uint8_t *)input,
        .inbuf_len = input_len,
        .outbuf = output,
    };
    jpeg_dec_header_info_t header = {0};

    ret = jpeg_dec_parse_header(decoder, &io, &header);
    if (ret != JPEG_ERR_OK) {
        goto exit;
    }
    if (header.width != width || header.height != height) {
        ESP_LOGW(TAG, "Drop JPEG with unexpected size: %ux%u, expected %ux%u",
                 header.width, header.height, width, height);
        ret = JPEG_ERR_INVALID_PARAM;
        goto exit;
    }

    int required_output_len = 0;
    ret = jpeg_dec_get_outbuf_len(decoder, &required_output_len);
    if (ret != JPEG_ERR_OK) {
        goto exit;
    }
    if ((size_t)required_output_len > output_len) {
        ESP_LOGW(TAG, "JPEG output too large: %d > %u", required_output_len, (unsigned)output_len);
        ret = JPEG_ERR_NO_MEM;
        goto exit;
    }

    ret = jpeg_dec_process(decoder, &io);

exit:
    jpeg_dec_close(decoder);
    return ret;
}
#endif

void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height)
{
    if (width != s_lcd_width || height != s_lcd_height) {
        ESP_LOGW(TAG, "Drop JPEG with unexpected size: %ux%u, expected %ux%u",
                 width, height, s_lcd_width, s_lcd_height);
        return;
    }

    static int fps_count;
    static int64_t start_time;
    if (start_time == 0) {
        start_time = esp_timer_get_time();
    }
    if (++fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;
    }

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
    uint32_t output_width;
    uint32_t output_height;
    esp_err_t layout_ret = app_lcd_get_jpeg_output_layout(buf, len, width, height,
                                                          &output_width, &output_height);
    if (layout_ret != ESP_OK) {
        ESP_LOGD(TAG, "Unsupported JPEG layout: %s", esp_err_to_name(layout_ret));
        return;
    }

    size_t expected_output_size = output_width * output_height * s_lcd_bytes_per_pixel;
    if (expected_output_size > s_decode_buffer_sizes[s_buffer_index]) {
        ESP_LOGW(TAG, "JPEG output too large: %u > %u", (unsigned)expected_output_size,
                 (unsigned)s_decode_buffer_sizes[s_buffer_index]);
        return;
    }

    uint32_t output_size = 0;
    esp_err_t decode_ret = jpeg_decoder_process(s_jpeg_decoder, &s_decode_config, buf, len,
                                                s_decode_buffers[s_buffer_index],
                                                s_decode_buffer_sizes[s_buffer_index], &output_size);
    if (decode_ret != ESP_OK) {
        ESP_LOGD(TAG, "JPEG decode failed: %s", esp_err_to_name(decode_ret));
        return;
    }
    if (output_size != expected_output_size) {
        ESP_LOGW(TAG, "Unexpected JPEG output size: %u, expected %u",
                 (unsigned)output_size, (unsigned)expected_output_size);
        return;
    }

    /*
     * The decoder writes MCU-padded rows. Remove the padding in place before
     * passing the visible rectangle to an LCD driver that expects packed rows.
     */
    if (output_width != width) {
        uint8_t *output = s_decode_buffers[s_buffer_index];
        size_t visible_row_size = width * s_lcd_bytes_per_pixel;
        size_t decoded_row_size = output_width * s_lcd_bytes_per_pixel;
        for (size_t row = 1; row < height; row++) {
            memmove(output + row * visible_row_size, output + row * decoded_row_size,
                    visible_row_size);
        }
    }
#else
    jpeg_error_t jpeg_ret = app_lcd_decode_jpeg(buf, len, s_decode_buffers[s_buffer_index],
                                                s_decode_buffer_sizes[s_buffer_index], width, height);
    if (jpeg_ret != JPEG_ERR_OK) {
        ESP_LOGD(TAG, "JPEG decode failed: %d", jpeg_ret);
        return;
    }
    if (s_lcd_frame_format == DEV_DISPLAY_LCD_FRAME_FORMAT_BGR888) {
        uint8_t *output = s_decode_buffers[s_buffer_index];
        size_t output_len = (size_t)width * height * s_lcd_bytes_per_pixel;
        for (size_t offset = 0; offset < output_len; offset += s_lcd_bytes_per_pixel) {
            uint8_t red = output[offset];
            output[offset] = output[offset + 2];
            output[offset + 2] = red;
        }
    }
#endif

    esp_err_t ret = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, width, height,
                                              s_decode_buffers[s_buffer_index]);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "LCD draw failed: %s", esp_err_to_name(ret));
        return;
    }

    s_buffer_index = (s_buffer_index + 1) % LCD_DECODE_BUFFER_COUNT;
}

esp_err_t app_lcd_get_resolution(uint16_t *width, uint16_t *height)
{
    ESP_RETURN_ON_FALSE(width && height, ESP_ERR_INVALID_ARG, TAG, "resolution output is NULL");
    ESP_RETURN_ON_FALSE(s_lcd_width && s_lcd_height, ESP_ERR_INVALID_STATE, TAG,
                        "display is not initialized");
    *width = s_lcd_width;
    *height = s_lcd_height;
    return ESP_OK;
}

esp_err_t app_lcd_init(void)
{
#if !CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT
    ESP_LOGE(TAG, "Selected Board Manager board has no display_lcd device");
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_FALSE(esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD),
                        ESP_ERR_NOT_FOUND, TAG, "selected board has no display_lcd device");
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD),
                        TAG, "initialize display_lcd failed");

    dev_display_lcd_handles_t *display = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD,
                                                            (void **)&display),
                        TAG, "get display_lcd handle failed");
    ESP_RETURN_ON_FALSE(display && display->panel_handle, ESP_ERR_INVALID_STATE, TAG,
                        "display_lcd returned an invalid panel handle");
    s_panel = display->panel_handle;

    dev_display_lcd_config_t *display_config = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD,
                                                            (void **)&display_config),
                        TAG, "get display_lcd config failed");

    s_lcd_width = display_config->swap_xy ? display_config->lcd_height : display_config->lcd_width;
    s_lcd_height = display_config->swap_xy ? display_config->lcd_width : display_config->lcd_height;
    ESP_RETURN_ON_FALSE(s_lcd_width && s_lcd_height, ESP_ERR_INVALID_SIZE, TAG,
                        "Board Manager returned an invalid display resolution");
    s_lcd_frame_format = display_config->frame_format;
    s_lcd_bytes_per_pixel = app_lcd_frame_format_bytes_per_pixel(s_lcd_frame_format);
    ESP_RETURN_ON_FALSE(s_lcd_bytes_per_pixel != 0, ESP_ERR_NOT_SUPPORTED, TAG,
                        "unsupported display frame format: %d", s_lcd_frame_format);
    ESP_LOGI(TAG, "Board display: %s (%s), resolution=%ux%u, framebuffer=%s",
             display_config->chip, display_config->sub_type, s_lcd_width, s_lcd_height,
             app_lcd_frame_format_name(s_lcd_frame_format));

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
    s_decode_config.output_format = s_lcd_bytes_per_pixel == 3 ?
                                    JPEG_DECODE_OUT_FORMAT_RGB888 : JPEG_DECODE_OUT_FORMAT_RGB565;
    s_decode_config.rgb_order = app_lcd_frame_format_uses_rgb_order(s_lcd_frame_format) ?
                                JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    jpeg_decode_engine_cfg_t engine_config = {
        .intr_priority = 1,
        .timeout_ms = 50,
    };
    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&engine_config, &s_jpeg_decoder),
                        TAG, "create JPEG decoder failed");
#endif

    ESP_RETURN_ON_ERROR(app_lcd_allocate_decode_buffers(), TAG, "allocate decode buffers failed");
    ESP_RETURN_ON_ERROR(app_lcd_backlight_set(100), TAG, "turn LCD backlight on failed");
    return ESP_OK;
#endif
}
