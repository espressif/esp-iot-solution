/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_painter.h"
#include "esp_io_expander_tca9554.h"
#include "esp_jpeg_dec.h"
#include "esp_timer.h"
#include "ppbuffer.h"
#include "usb_stream.h"

static const char *TAG = "uvc_mic_spk_demo";
/****************** configure the example working mode *******************************/

#define ENABLE_UVC_FRAME_RESOLUTION_ANY    1               /* Using any resolution found from the camera */
#if (ENABLE_UVC_FRAME_RESOLUTION_ANY)
#define DEMO_UVC_FRAME_WIDTH               FRAME_RESOLUTION_ANY
#define DEMO_UVC_FRAME_HEIGHT              FRAME_RESOLUTION_ANY
#define DISPLAY_BUFFER_SIZE                0
#else
#define DEMO_UVC_FRAME_WIDTH               480
#define DEMO_UVC_FRAME_HEIGHT              320
#define DISPLAY_BUFFER_SIZE                DEMO_UVC_FRAME_WIDTH * DEMO_UVC_FRAME_HEIGHT * 2
#endif
#define DEMO_UVC_XFER_BUFFER_SIZE          ( 45 * 1024) //Double buffer

static esp_painter_handle_t painter        = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint8_t *rgb_frame_buf1             = NULL;
static uint8_t *rgb_frame_buf2             = NULL;
static uint8_t *cur_frame_buf              = NULL;
static uint8_t *jpg_frame_buf1             = NULL;
static uint8_t *jpg_frame_buf2             = NULL;
static PingPongBuffer_t *ppbuffer_handle   = NULL;
static jpeg_dec_io_t *jpeg_io              = NULL;
static jpeg_dec_header_info_t *out_info    = NULL;

static uint16_t current_width  = DEMO_UVC_FRAME_WIDTH;
static uint16_t current_height = DEMO_UVC_FRAME_HEIGHT;
static bool if_ppbuffer_init   = false;

#if CONFIG_BSP_LCD_SUB_BOARD_480_480
static esp_io_expander_handle_t io_expander = NULL; // IO expander tca9554 handle
#endif
extern esp_lcd_panel_handle_t bsp_lcd_init(void *arg);

static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    esp_err_t ret = ESP_OK;
    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    jpeg_dec = jpeg_dec_open(&config);

    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;
    int inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + inbuf_consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0) {
        goto _exit;
    }

_exit:
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    return ret;
}

static void adaptive_jpg_frame_buffer(size_t length)
{
    if (jpg_frame_buf1 != NULL) {
        free(jpg_frame_buf1);
    }

    if (jpg_frame_buf2 != NULL) {
        free(jpg_frame_buf2);
    }

    jpg_frame_buf1 = (uint8_t *)heap_caps_aligned_alloc(16, length, MALLOC_CAP_SPIRAM);
    assert(jpg_frame_buf1 != NULL);
    jpg_frame_buf2 = (uint8_t *)heap_caps_aligned_alloc(16, length, MALLOC_CAP_SPIRAM);
    assert(jpg_frame_buf2 != NULL);
    ESP_ERROR_CHECK(ppbuffer_create(ppbuffer_handle, jpg_frame_buf2, jpg_frame_buf1));
    if_ppbuffer_init = true;
}

static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    ESP_LOGI(TAG, "uvc callback! frame_format = %d, seq = %"PRIu32", width = %"PRIu32", height = %"PRIu32", length = %u, ptr = %d",
             frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);
    if (current_width != frame->width || current_height != frame->height ) {
        current_width = frame->width;
        current_height = frame->height;
        adaptive_jpg_frame_buffer(current_width * current_height * 2);
        memset(rgb_frame_buf1, 0, BSP_LCD_H_RES * BSP_LCD_V_RES * 2);
        memset(rgb_frame_buf2, 0, BSP_LCD_H_RES * BSP_LCD_V_RES * 2);
    }

    static void *jpeg_buffer = NULL;
    assert(jpeg_buffer != NULL);
    ppbuffer_get_write_buf(ppbuffer_handle, &jpeg_buffer);
    esp_jpeg_decoder_one_picture((uint8_t *)frame->data, frame->data_bytes, jpeg_buffer);
    ppbuffer_set_write_done(ppbuffer_handle);
}

static void _display_task(void *arg)
{
    uint16_t *lcd_buffer = NULL;
    int64_t count_start_time = 0;
    int frame_count = 0;
    int fps = 0;
    int x_start = 0;
    int y_start = 0;
    int width = 0;
    int height = 0;
    int x_jump = 0;
    int y_jump = 0;

    while ( !if_ppbuffer_init ) {
        vTaskDelay(1);
    }

    while (1) {
        if ( ppbuffer_get_read_buf(ppbuffer_handle, (void *)&lcd_buffer) == ESP_OK) {
            if (count_start_time == 0) {
                count_start_time = esp_timer_get_time();
            }
            if (++frame_count == 20) {
                frame_count = 0;
                fps = 20 * 1000000 / (esp_timer_get_time() - count_start_time);
                count_start_time = esp_timer_get_time();
                ESP_LOGI(TAG, "camera fps: %d", fps);
            }

            if (current_width <= BSP_LCD_H_RES && current_height <= BSP_LCD_V_RES) {
                x_start = (BSP_LCD_H_RES - current_width) / 2;
                y_start = (BSP_LCD_V_RES - current_height) / 2;
                esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 1, 1, cur_frame_buf);
                esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_start + current_width, y_start + current_height, lcd_buffer);
            } else {
                if (current_width < BSP_LCD_H_RES ) {
                    width = current_width;
                    x_start = (BSP_LCD_H_RES - current_width) / 2;
                    x_jump = 0;
                } else {
                    width = BSP_LCD_H_RES;
                    x_start = 0;
                    x_jump = (current_width - BSP_LCD_H_RES) / 2;
                }

                if (current_height < BSP_LCD_V_RES ) {
                    height = current_height;
                    y_start = (BSP_LCD_V_RES - current_height) / 2;
                    y_jump = 0;
                } else {
                    height = BSP_LCD_V_RES;
                    y_start = 0;
                    y_jump = (current_height - BSP_LCD_V_RES) / 2;
                }

                esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 1, 1, cur_frame_buf);
                for (int i = y_start; i < height; i++) {
                    esp_lcd_panel_draw_bitmap(panel_handle, x_start, i, x_start + width, i + 1, &lcd_buffer[(y_jump + i)*current_width + x_jump] );
                }
            }
            esp_painter_draw_string_format(painter, x_start, y_start, NULL, COLOR_BRUSH_DEFAULT, "FPS: %d %d*%d", fps, current_width, current_height);
            cur_frame_buf = (cur_frame_buf == rgb_frame_buf1) ? rgb_frame_buf2 : rgb_frame_buf1;
            ppbuffer_set_read_done(ppbuffer_handle);
        }
        vTaskDelay(1);
    }
}

static esp_err_t _display_init(void)
{
    bsp_i2c_init();
    void *lcd_usr_data = NULL;
#if CONFIG_BSP_LCD_SUB_BOARD_480_480
    /* Sub board 2 with 480x480 uses io expander to configure LCD */
    io_expander = bsp_io_expander_init();
    lcd_usr_data = io_expander;
#endif
    panel_handle = bsp_lcd_init(lcd_usr_data);
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
        },
        .default_font = &esp_painter_basic_font_24,
        .piexl_color_byte = 2,
        .lcd_panel = panel_handle,
    };
    ESP_ERROR_CHECK(esp_painter_new(&painter_config, &painter));
    xTaskCreate(_display_task, "_display", 4 * 1024, NULL, 5, NULL);
    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_err_t ret = ESP_FAIL;

    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    /* malloc frame buffer for ppbuffer_handle*/
    ppbuffer_handle = (PingPongBuffer_t *)malloc(sizeof(PingPongBuffer_t));

    // Create io_callback handle
    jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    assert(jpeg_io != NULL);

    // Create out_info handle
    out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    assert(out_info != NULL);

    /* malloc double buffer for lcd frame, Must be 16-byte aligned */
    if (DISPLAY_BUFFER_SIZE) {
        adaptive_jpg_frame_buffer(DISPLAY_BUFFER_SIZE);
    }

    ESP_ERROR_CHECK(_display_init());

    uvc_config_t uvc_config = {
        .frame_width = DEMO_UVC_FRAME_WIDTH,
        .frame_height = DEMO_UVC_FRAME_HEIGHT,
        .frame_interval = FPS2INTERVAL(15),
        .xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = &camera_frame_cb,
        .frame_cb_arg = NULL,
    };

    ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }

    /* Start stream with pre-configs, usb stream driver will create multi-tasks internal
    to handle usb data from different pipes, and user's callback will be called after new frame ready. */
    ret = usb_streaming_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb streaming start failed");
    }

    ESP_LOGI(TAG, "usb streaming start succeed");

    while (1) {
        vTaskDelay(100);
    }
}
