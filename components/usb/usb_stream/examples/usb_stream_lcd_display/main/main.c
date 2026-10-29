/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_painter.h"
#include "esp_io_expander_tca9554.h"
#include "esp_jpeg_dec.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ppbuffer.h"
#include "usb_stream.h"
#include "iot_button.h"

static const char *TAG = "uvc_camera_lcd_demo";
/****************** configure the example working mode *******************************/

#define DEMO_UVC_XFER_BUFFER_SIZE            ( 88 * 1024) //Double buffer
#define DEMO_KEY_RESOLUTION                  "resolution"
#define DEMO_SWITCH_BUTTON_IO                0

#define BIT0_FRAME_START     (0x01 << 0)
static EventGroupHandle_t s_evt_handle;

typedef struct {
    uint16_t width;
    uint16_t height;
} camera_frame_size_t;

typedef struct {
    camera_frame_size_t camera_frame_size;
    uvc_frame_size_t *camera_frame_list;
    size_t camera_frame_list_num;
    size_t camera_currect_frame_index;
} camera_resolution_info_t;

static camera_resolution_info_t camera_resolution_info = {0};
static esp_painter_handle_t painter                    = NULL;
static esp_lcd_panel_handle_t panel_handle             = NULL;
static uint8_t *rgb_frame_buf1                         = NULL;
static uint8_t *rgb_frame_buf2                         = NULL;
static uint8_t *cur_frame_buf                          = NULL;
static uint8_t *jpg_frame_buf1                         = NULL;
static uint8_t *jpg_frame_buf2                         = NULL;
static uint8_t *xfer_buffer_a                          = NULL;
static uint8_t *xfer_buffer_b                          = NULL;
static uint8_t *frame_buffer                           = NULL;
static PingPongBuffer_t *ppbuffer_handle               = NULL;
static uint16_t current_width                          = 0;
static uint16_t current_height                         = 0;
static bool if_ppbuffer_init                           = false;

static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    esp_err_t ret = ESP_OK;
    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    jpeg_dec = jpeg_dec_open(&config);

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
    free(out_info);
    free(jpeg_io);
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
    if (current_width != frame->width || current_height != frame->height) {
        current_width = frame->width;
        current_height = frame->height;
        adaptive_jpg_frame_buffer(current_width * current_height * 2);
        memset(rgb_frame_buf1, 0, BSP_LCD_H_RES * BSP_LCD_V_RES * 2);
        memset(rgb_frame_buf2, 0, BSP_LCD_H_RES * BSP_LCD_V_RES * 2);
    }

    static void *jpeg_buffer = NULL;

    /* A buffer equal to the screen size can be directly written into the LCD buffer to improve the frame rate */
    if (current_width == BSP_LCD_H_RES && current_height <= BSP_LCD_V_RES) {
        size_t length = (BSP_LCD_V_RES - current_height) * BSP_LCD_V_RES ;
        esp_jpeg_decoder_one_picture((uint8_t *)frame->data, frame->data_bytes, cur_frame_buf + length);
    } else {
        ppbuffer_get_write_buf(ppbuffer_handle, &jpeg_buffer);
        assert(jpeg_buffer != NULL);
        esp_jpeg_decoder_one_picture((uint8_t *)frame->data, frame->data_bytes, jpeg_buffer);
    }
    ppbuffer_set_write_done(ppbuffer_handle);
    vTaskDelay(pdMS_TO_TICKS(1));
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

    while (!if_ppbuffer_init) {
        vTaskDelay(1);
    }

    while (1) {
        if (ppbuffer_get_read_buf(ppbuffer_handle, (void *)&lcd_buffer) == ESP_OK) {
            if (current_width == BSP_LCD_H_RES && current_height <= BSP_LCD_V_RES) {
                x_start = 0;
                y_start = (BSP_LCD_V_RES - current_height) / 2;
                esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, current_width, current_height, cur_frame_buf);
            } else if (current_width < BSP_LCD_H_RES && current_height < BSP_LCD_V_RES) {
                x_start = (BSP_LCD_H_RES - current_width) / 2;
                y_start = (BSP_LCD_V_RES - current_height) / 2;
                esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 1, 1, cur_frame_buf);
                esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_start + current_width, y_start + current_height, lcd_buffer);
            } else {
                /* This section is for refreshing images with a resolution larger than the screen, which is currently not enabled */
                if (current_width < BSP_LCD_H_RES) {
                    width = current_width;
                    x_start = (BSP_LCD_H_RES - current_width) / 2;
                    x_jump = 0;
                } else {
                    width = BSP_LCD_H_RES;
                    x_start = 0;
                    x_jump = (current_width - BSP_LCD_H_RES) / 2;
                }

                if (current_height < BSP_LCD_V_RES) {
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
                    esp_lcd_panel_draw_bitmap(panel_handle, x_start, i, x_start + width, i + 1, &lcd_buffer[(y_jump + i)*current_width + x_jump]);
                }
            }

            ppbuffer_set_read_done(ppbuffer_handle);
            if (count_start_time == 0) {
                count_start_time = esp_timer_get_time();
            }

            if (++frame_count == 20) {
                frame_count = 0;
                fps = 20 * 1000000 / (esp_timer_get_time() - count_start_time);
                count_start_time = esp_timer_get_time();
                ESP_LOGI(TAG, "camera fps: %d %d*%d", fps, current_width, current_height);
            }

            esp_painter_draw_string_format(painter, x_start, y_start, NULL, COLOR_BRUSH_DEFAULT, "FPS: %d %d*%d", fps, current_width, current_height);
            cur_frame_buf = (cur_frame_buf == rgb_frame_buf1) ? rgb_frame_buf2 : rgb_frame_buf1;
        }
        vTaskDelay(1);
    }
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
        },
        .default_font = &esp_painter_basic_font_24,
        .piexl_color_byte = 2,
        .lcd_panel = panel_handle,
    };
    ESP_ERROR_CHECK(esp_painter_new(&painter_config, &painter));
    xTaskCreate(_display_task, "_display", 4 * 1024, NULL, 5, NULL);
    return ESP_OK;
}

static void _get_value_from_nvs(char *key, void *value, size_t *size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        err = nvs_get_blob(my_handle, key, value, size);
        switch (err) {
        case ESP_OK:
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "%s is not initialized yet!", key);
            break;
        default :
            ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        }

        nvs_close(my_handle);
    }
}

static esp_err_t _set_value_to_nvs(char *key, void *value, size_t size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        err = nvs_set_blob(my_handle, key, value, size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS set failed %s", esp_err_to_name(err));
        }

        err = nvs_commit(my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS commit failed");
        }

        nvs_close(my_handle);
    }

    return err;
}

static esp_err_t _usb_stream_init(void)
{
    uvc_config_t uvc_config = {
        .frame_interval = FRAME_INTERVAL_FPS_30,
        .xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = &camera_frame_cb,
        .frame_cb_arg = NULL,
        .frame_width = FRAME_RESOLUTION_ANY,
        .frame_height = FRAME_RESOLUTION_ANY,
        .flags = FLAG_UVC_SUSPEND_AFTER_START,
    };

    esp_err_t ret = uvc_streaming_config(&uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }
    return ret;
}

static size_t _find_current_resolution(camera_frame_size_t *camera_frame_size)
{
    if (camera_resolution_info.camera_frame_list == NULL) {
        return -1;
    }

    size_t i = 0;
    while (i < camera_resolution_info.camera_frame_list_num) {
        if (camera_frame_size->width >= camera_resolution_info.camera_frame_list[i].width && camera_frame_size->height >= camera_resolution_info.camera_frame_list[i].height) {
            /* Find next resolution
               If current resolution is the min resolution, switch to the max resolution*/
            camera_frame_size->width = camera_resolution_info.camera_frame_list[i].width;
            camera_frame_size->height = camera_resolution_info.camera_frame_list[i].height;
            break;
        } else if (i == camera_resolution_info.camera_frame_list_num - 1) {
            camera_frame_size->width = camera_resolution_info.camera_frame_list[i].width;
            camera_frame_size->height = camera_resolution_info.camera_frame_list[i].height;
            break;
        }
        i++;
    }
    ESP_LOGI(TAG, "Current resolution is %dx%d", camera_frame_size->width, camera_frame_size->height);
    return i;
}

static void switch_button_press_down_cb(void *arg, void *data)
{
    if (camera_resolution_info.camera_frame_list == NULL || xEventGroupWaitBits(s_evt_handle, BIT0_FRAME_START, false, false, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    ESP_LOGI(TAG, "old resolution is %d*%d", camera_resolution_info.camera_frame_size.width, camera_resolution_info.camera_frame_size.height);
    if (++camera_resolution_info.camera_currect_frame_index >= camera_resolution_info.camera_frame_list_num) {
        camera_resolution_info.camera_currect_frame_index = 0;
    }
    camera_resolution_info.camera_frame_size.width = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width;
    camera_resolution_info.camera_frame_size.height = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height;
    ESP_LOGI(TAG, "current resolution is %d*%d", camera_resolution_info.camera_frame_size.width, camera_resolution_info.camera_frame_size.height);

    /* Save the new camera resolution to nvs */
    usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL);
    ESP_ERROR_CHECK(uvc_frame_size_reset(camera_resolution_info.camera_frame_size.width,
                                         camera_resolution_info.camera_frame_size.height,
                                         FPS2INTERVAL(30)));
    ESP_ERROR_CHECK(_set_value_to_nvs(DEMO_KEY_RESOLUTION, &camera_resolution_info.camera_frame_size, sizeof(camera_frame_size_t)));
    esp_lcd_rgb_panel_restart(panel_handle);
    usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
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
        ESP_LOGE(TAG, "button register callback fail");
    }
    return ret;
}

static void _stream_state_changed_cb(usb_stream_state_t event, void *arg)
{
    switch (event) {
    case STREAM_CONNECTED: {
        /* Get camera resolution in nvs*/
        size_t size = sizeof(camera_frame_size_t);
        _get_value_from_nvs(DEMO_KEY_RESOLUTION, &camera_resolution_info.camera_frame_size, &size);
        size_t frame_index = 0;
        uvc_frame_size_list_get(NULL, &camera_resolution_info.camera_frame_list_num, NULL);
        if (camera_resolution_info.camera_frame_list_num) {
            ESP_LOGI(TAG, "UVC: get frame list size = %u, current = %u", camera_resolution_info.camera_frame_list_num, frame_index);
            uvc_frame_size_t *_frame_list = (uvc_frame_size_t *)malloc(camera_resolution_info.camera_frame_list_num * sizeof(uvc_frame_size_t));

            camera_resolution_info.camera_frame_list = (uvc_frame_size_t *)realloc(camera_resolution_info.camera_frame_list, camera_resolution_info.camera_frame_list_num * sizeof(uvc_frame_size_t));
            if (NULL == camera_resolution_info.camera_frame_list) {
                ESP_LOGE(TAG, "camera_resolution_info.camera_frame_list");
            }
            uvc_frame_size_list_get(_frame_list, NULL, NULL);
            for (size_t i = 0; i < camera_resolution_info.camera_frame_list_num; i++) {
                if (_frame_list[i].width <= BSP_LCD_H_RES && _frame_list[i].height <= BSP_LCD_V_RES) {
                    camera_resolution_info.camera_frame_list[frame_index++] = _frame_list[i];
                    ESP_LOGI(TAG, "\tpick frame[%u] = %ux%u", i, _frame_list[i].width, _frame_list[i].height);
                } else {
                    ESP_LOGI(TAG, "\tdrop frame[%u] = %ux%u", i, _frame_list[i].width, _frame_list[i].height);
                }
            }
            camera_resolution_info.camera_frame_list_num = frame_index;
            if (camera_resolution_info.camera_frame_size.width != 0 && camera_resolution_info.camera_frame_size.height != 0) {
                camera_resolution_info.camera_currect_frame_index = _find_current_resolution(&camera_resolution_info.camera_frame_size);
            } else {
                camera_resolution_info.camera_currect_frame_index = 0;
            }

            if (-1 == camera_resolution_info.camera_currect_frame_index) {
                ESP_LOGE(TAG, "fine current resolution fail");
                break;
            }
            ESP_ERROR_CHECK(uvc_frame_size_reset(camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width,
                                                 camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height, FPS2INTERVAL(30)));
            camera_frame_size_t camera_frame_size = {
                .width = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width,
                .height = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height,
            };
            ESP_ERROR_CHECK(_set_value_to_nvs(DEMO_KEY_RESOLUTION, &camera_frame_size, sizeof(camera_frame_size_t)));

            if (_frame_list != NULL) {
                free(_frame_list);
            }
            /* Wait USB Camera connected */
            usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
            xEventGroupSetBits(s_evt_handle, BIT0_FRAME_START);
        } else {
            ESP_LOGW(TAG, "UVC: get frame list size = %u", camera_resolution_info.camera_frame_list_num);
        }
        ESP_LOGI(TAG, "Device connected");
        break;
    }
    case STREAM_DISCONNECTED:
        xEventGroupClearBits(s_evt_handle, BIT0_FRAME_START);
        ESP_LOGI(TAG, "Device disconnected");
        break;
    default:
        ESP_LOGE(TAG, "Unknown event");
        break;
    }
}

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());
        /* Retry nvs_flash_init */
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Create event group */
    s_evt_handle = xEventGroupCreate();
    if (s_evt_handle == NULL) {
        ESP_LOGE(TAG, "line-%u event group create failed", __LINE__);
        assert(0);
    }

    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    xfer_buffer_a = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    xfer_buffer_b = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    frame_buffer = (uint8_t *)malloc(DEMO_UVC_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    /* malloc frame buffer for ppbuffer_handle*/
    ppbuffer_handle = (PingPongBuffer_t *)malloc(sizeof(PingPongBuffer_t));

    /* Initialize the screen */
    ESP_ERROR_CHECK(_display_init());

    /* Initialize the button to switch resolution */
    ESP_ERROR_CHECK(_switch_button_init());

    /* Initialize the screen according to the resolution stored in nvs */
    ESP_ERROR_CHECK(_usb_stream_init());

    /* Register a callback to control uvc frame size */
    ESP_ERROR_CHECK(usb_streaming_state_register(&_stream_state_changed_cb, NULL));

    /* Start stream with pre-configs, usb stream driver will create multi-tasks internal
    to handle usb data from different pipes, and user's callback will be called after new frame ready. */
    ESP_ERROR_CHECK(usb_streaming_start());
    ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));
}
