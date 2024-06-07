/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_psram.h"
#include "esp_dma_utils.h"
#include "usb_device_uvc.h"
#include "esp_lcd_usb_display.h"

#define USB_DISPLAY_SWAP_XY    (0)
#define USB_DISPLAY_MIRROR_Y   (1)
#define USB_DISPLAY_MIRROR_X   (2)

typedef enum {
    ROTATE_MASK_SWAP_XY = BIT(USB_DISPLAY_SWAP_XY),
    ROTATE_MASK_MIRROR_Y = BIT(USB_DISPLAY_MIRROR_Y),
    ROTATE_MASK_MIRROR_X = BIT(USB_DISPLAY_MIRROR_X),
} panel_rotate_mask_t;

static const char *TAG = "usb_display";

static esp_err_t panel_usb_display_del(esp_lcd_panel_t *panel);
static esp_err_t panel_usb_display_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_usb_display_init(esp_lcd_panel_t *panel);
static esp_err_t panel_usb_display_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_usb_display_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_usb_display_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_usb_display_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_usb_display_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_usb_display_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    uint16_t h_res;
    uint16_t v_res;
    uint8_t bytes_per_pixel;
    void *user_ctx;                 // Reserved user's data of callback functions
    // RGB
    size_t fb_rgb_size;
    uint8_t fb_rgb_nums;
    uint8_t fb_rgb_index;
    void *fb_rgb[CONFIG_FRAME_BUF_RGB_MAX_NUM];
    uint16_t x_gap;
    uint16_t y_gap;
    int rotate_mask;                // panel rotate_mask mask, Or'ed of `panel_rotate_mask_t`
    // UVC
    void *fb_uvc;
    int uvc_device_index;
    void (*on_uvc_start)(void *user_ctx);
    void (*on_uvc_stop)(void *user_ctx);
    void (*on_display_trans_done)(void *user_ctx);
    // JPEG
    uvc_fb_t fb_uvc_jpeg;
    size_t fb_uvc_jpeg_size;
    jpeg_encode_cfg_t enc_config;
    void *jpeg_encode_handle;
    TaskHandle_t jpeg_encode_task_handle;
    SemaphoreHandle_t sem_jpeg_encode_ready;
    SemaphoreHandle_t sem_jpeg_encode_finish;
    uint8_t jpeg_encode_task_priority;
    int jpeg_encode_task_core;
    esp_lcd_jpeg_buf_finish_cb_t *on_jpeg_encode_done;
} usb_display_t;

static esp_err_t alloc_buffer_memory(usb_display_t *usb_display, usb_display_vendor_config_t *vendor_config);

esp_err_t esp_lcd_new_panel_usb_display(usb_display_vendor_config_t *vendor_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_USB_DISPLAY_VER_MAJOR,
             ESP_LCD_USB_DISPLAY_VER_MINOR,
             ESP_LCD_USB_DISPLAY_VER_PATCH);

    ESP_RETURN_ON_FALSE(vendor_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    usb_display_t *usb_display = NULL;
    usb_display = calloc(1, sizeof(usb_display_t));
    ESP_GOTO_ON_FALSE(usb_display, ESP_ERR_NO_MEM, err, TAG, "no mem for virtual display panel");
    uint8_t bytes_per_pixel = 0;

    jpeg_encode_cfg_t enc_config = {
        .sub_sample = vendor_config->jpeg_encode_config.sub_sample,
        .image_quality = vendor_config->jpeg_encode_config.quality,
        .width = vendor_config->h_res,
        .height = vendor_config->v_res,
    };

    switch (vendor_config->bits_per_pixel) {
    case 8:     // GRAY
        bytes_per_pixel = 1;
        enc_config.src_type = JPEG_ENC_SRC_GRAY;
        if (enc_config.sub_sample != JPEG_DOWN_SAMPLING_GRAY) {
            enc_config.sub_sample = JPEG_DOWN_SAMPLING_GRAY;
            ESP_LOGE(TAG, "The original format is GRAY. The JPEG encoder only supports GRAY downsampling. It has been automatically converted.");
        }
        break;
    case 16:    // RGB565
        bytes_per_pixel = 2;
        enc_config.src_type = JPEG_ENC_SRC_RGB565;
        break;
    case 24:    // RGB888
        bytes_per_pixel = 3;
        enc_config.src_type = JPEG_ENC_SRC_RGB888;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    jpeg_encoder_handle_t jpge_handle = NULL;
    jpeg_encode_engine_cfg_t encode_eng_cfg = {
        .timeout_ms = 70,
    };
    ESP_ERROR_CHECK(jpeg_new_encoder_engine(&encode_eng_cfg, &jpge_handle));

    usb_display->h_res = vendor_config->h_res;
    usb_display->v_res = vendor_config->v_res;
    usb_display->bytes_per_pixel = bytes_per_pixel;
    usb_display->user_ctx = vendor_config->user_ctx;
    usb_display->fb_rgb_nums = (vendor_config->fb_rgb_nums > CONFIG_FRAME_BUF_RGB_MAX_NUM) ? CONFIG_FRAME_BUF_RGB_MAX_NUM : vendor_config->fb_rgb_nums;
    usb_display->fb_rgb_index = 0;
    usb_display->uvc_device_index = vendor_config->uvc_device_index;
    usb_display->fb_uvc_jpeg_size = vendor_config->fb_uvc_jpeg_size;
    usb_display->enc_config = enc_config;
    usb_display->jpeg_encode_handle = jpge_handle;
    usb_display->jpeg_encode_task_priority = vendor_config->jpeg_encode_config.task_priority;
    int core = vendor_config->jpeg_encode_config.task_core_id;
    usb_display->jpeg_encode_task_core = (core == -1) ? tskNO_AFFINITY : core;
    usb_display->on_uvc_start = vendor_config->on_uvc_start;
    usb_display->on_uvc_stop = vendor_config->on_uvc_stop;
    usb_display->on_display_trans_done = vendor_config->on_display_trans_done;

    ESP_GOTO_ON_ERROR(alloc_buffer_memory(usb_display, vendor_config), err, TAG, "alloc buffer memory failed");

    usb_display->base.del = panel_usb_display_del;
    usb_display->base.reset = panel_usb_display_reset;
    usb_display->base.init = panel_usb_display_init;
    usb_display->base.draw_bitmap = panel_usb_display_draw_bitmap;
    usb_display->base.invert_color = panel_usb_display_invert_color;
    usb_display->base.set_gap = panel_usb_display_set_gap;
    usb_display->base.mirror = panel_usb_display_mirror;
    usb_display->base.swap_xy = panel_usb_display_swap_xy;
    usb_display->base.disp_on_off = panel_usb_display_disp_on_off;
    *ret_panel = &(usb_display->base);
    ESP_LOGD(TAG, "new usb_display panel @%p", usb_display);

    return ESP_OK;

err:
    free(usb_display);
    return ret;
}

static esp_err_t alloc_buffer_memory(usb_display_t *usb_display, usb_display_vendor_config_t *vendor_config)
{
    esp_err_t ret = ESP_OK;
    size_t fb_rgb_size = usb_display->h_res * usb_display->v_res * usb_display->bytes_per_pixel;
    size_t fb_uvc_jpeg_size = vendor_config->fb_uvc_jpeg_size;
    size_t tx_buffer_size = 0;
    size_t rx_buffer_size = 0;

    jpeg_encode_memory_alloc_cfg_t rx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    jpeg_encode_memory_alloc_cfg_t tx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
    };

    // alloc frame buffer
    for (int i = 0; i < usb_display->fb_rgb_nums; i++) {
        usb_display->fb_rgb[i] = jpeg_alloc_encoder_mem(fb_rgb_size, &tx_mem_cfg, &tx_buffer_size);
        ESP_GOTO_ON_FALSE(usb_display->fb_rgb[i], ESP_ERR_NO_MEM, err, TAG, "no mem for RGB frame buffer");
        ESP_LOGD(TAG, "RGB frame buffer size tx_buffer_size: %d", tx_buffer_size);
    }

    size_t fb_size = 0;
    esp_dma_mem_info_t dma_mem_info = {
        .extra_heap_caps = MALLOC_CAP_SPIRAM,
    };
    ESP_RETURN_ON_ERROR(esp_dma_capable_calloc(1, fb_uvc_jpeg_size, &dma_mem_info, &usb_display->fb_uvc, &fb_size), TAG, "no mem for UVC frame buffer");
    ESP_LOGD(TAG, "UVC frame buffer size: %d", fb_size);

    usb_display->fb_uvc_jpeg.buf = jpeg_alloc_encoder_mem(usb_display->h_res * usb_display->v_res, &rx_mem_cfg, &rx_buffer_size);
    ESP_GOTO_ON_FALSE(usb_display->fb_uvc_jpeg.buf, ESP_ERR_NO_MEM, err, TAG, "no mem for JPEG frame buffer");
    ESP_LOGD(TAG, "JPEG frame buffer size rx_buffer_size: %d", rx_buffer_size);

    usb_display->fb_rgb_size = fb_rgb_size;
    usb_display->fb_uvc_jpeg.width = usb_display->h_res;
    usb_display->fb_uvc_jpeg.height = usb_display->v_res;
    usb_display->fb_uvc_jpeg.format = UVC_FORMAT_JPEG;

    return ESP_OK;

err:
    for (int i = 0; i < usb_display->fb_rgb_nums; i++) {
        free(usb_display->fb_rgb[i]);
    }
    free(usb_display->fb_uvc_jpeg.buf);
    return ret;
}

esp_err_t esp_lcd_usb_display_get_frame_buffer(esp_lcd_panel_handle_t handle, uint32_t fb_num, void **fb0, ...)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    usb_display_t *panel = __containerof(handle, usb_display_t, base);
    ESP_RETURN_ON_FALSE(fb_num && fb_num <= panel->fb_rgb_nums, ESP_ERR_INVALID_ARG, TAG, "frame buffer num out of range(< %d)", panel->fb_rgb_nums);

    void **fb_itor = fb0;
    va_list args;
    va_start(args, fb0);
    for (int i = 0; i < fb_num; i++) {
        if (fb_itor) {
            *fb_itor = panel->fb_rgb[i];
            fb_itor = va_arg(args, void **);
        }
    }
    va_end(args);
    return ESP_OK;
}

static void usb_uvc_stop_cb(void *arg)
{
    ESP_LOGI(TAG, "USB UVC stop");

    usb_display_t *usb_display = (usb_display_t *)arg;
    if (usb_display->on_uvc_stop) {
        usb_display->on_uvc_stop(usb_display->user_ctx);
    }
}

static esp_err_t usb_uvc_start_cb(uvc_format_t format, int width, int height, int rate, void *arg)
{
    ESP_LOGI(TAG, "USB UVC start, format: %d, width: %d, height: %d, rate: %d", format, width, height, rate);

    usb_display_t *usb_display = (usb_display_t *)arg;
    if (usb_display->on_uvc_start) {
        usb_display->on_uvc_start(usb_display->user_ctx);
    }
    return ESP_OK;
}

static uvc_fb_t* usb_uvc_get_cb(void *arg)
{
    ESP_LOGD(TAG, "USB UVC get Buffer");
    usb_display_t *usb_display = (usb_display_t *)arg;

    if (xSemaphoreTake(usb_display->sem_jpeg_encode_finish, portMAX_DELAY) == pdTRUE) {
        ESP_LOGD(TAG, "JPEG encode done, size: %d", usb_display->fb_uvc_jpeg.len);
        return &usb_display->fb_uvc_jpeg;
    } else {
        return NULL;
    }
}

static void usb_uvc_return_cb(uvc_fb_t *fb, void *arg)
{
    ESP_LOGD(TAG, "UVC Return Buffer");
    usb_display_t *usb_display = (usb_display_t *)arg;

    if (usb_display->on_display_trans_done) {
        usb_display->on_display_trans_done(usb_display->user_ctx);
    }
    xSemaphoreGive(usb_display->sem_jpeg_encode_ready);
}

static void jpeg_encode_task(void *arg)
{
    ESP_LOGI(TAG, "JPEG encode task start");
    esp_err_t ret = ESP_OK;

    usb_display_t *usb_display = (usb_display_t *)arg;
    bool need_yield = false;

    while (1) {
        if (xSemaphoreTake(usb_display->sem_jpeg_encode_ready, portMAX_DELAY) == pdTRUE) {
            ret = jpeg_encoder_process(usb_display->jpeg_encode_handle, &usb_display->enc_config, usb_display->fb_rgb[usb_display->fb_rgb_index], usb_display->fb_rgb_size, usb_display->fb_uvc_jpeg.buf, usb_display->fb_uvc_jpeg_size, (uint32_t *)&usb_display->fb_uvc_jpeg.len);
            ESP_LOGD(TAG, "JPEG encode done, size: %d", usb_display->fb_uvc_jpeg.len);

            need_yield = false;
            if (ret == ESP_OK && usb_display->on_jpeg_encode_done) {
                if (usb_display->on_jpeg_encode_done(usb_display->user_ctx)) {
                    need_yield = true;
                }
            }

            if (need_yield) {
                portYIELD();
            }

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "JPEG encode failed");
            } else {
                vTaskDelay(pdMS_TO_TICKS(CONFIG_JPEG_ENC_TASK_MIN_DELAY_MS));
                xSemaphoreGive(usb_display->sem_jpeg_encode_finish);
            }
        }
    }
}

static esp_err_t panel_usb_display_init(esp_lcd_panel_t *panel)
{
    usb_display_t *usb_display = __containerof(panel, usb_display_t, base);

    ESP_LOGD(TAG, "initialize UVC device");
    uvc_device_config_t uvc_dev_config = {
        .uvc_buffer = usb_display->fb_uvc,
        .uvc_buffer_size = usb_display->fb_uvc_jpeg_size,
        .start_cb = usb_uvc_start_cb,
        .fb_get_cb = usb_uvc_get_cb,
        .fb_return_cb = usb_uvc_return_cb,
        .stop_cb = usb_uvc_stop_cb,
        .cb_ctx = usb_display,
    };

    ESP_RETURN_ON_ERROR(uvc_device_config(usb_display->uvc_device_index, &uvc_dev_config), TAG, "UVC device config failed");
    ESP_RETURN_ON_ERROR(uvc_device_init(), TAG, "UVC device init failed");

    ESP_LOGD(TAG, "create JPEG encode task");
    usb_display->sem_jpeg_encode_ready = xSemaphoreCreateBinary();
    usb_display->sem_jpeg_encode_finish = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(usb_display->sem_jpeg_encode_ready && usb_display->sem_jpeg_encode_finish, ESP_ERR_NO_MEM,
                        TAG, "no mem for semaphore");
    xSemaphoreGive(usb_display->sem_jpeg_encode_ready);

    BaseType_t ret = xTaskCreatePinnedToCore(jpeg_encode_task, "JPEG Encode", 4 * 1024, usb_display,
                                             usb_display->jpeg_encode_task_priority, &usb_display->jpeg_encode_task_handle,
                                             usb_display->jpeg_encode_task_core);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "JPEG encode task create failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t panel_usb_display_del(esp_lcd_panel_t *panel)
{
    ESP_LOGE(TAG, "delete is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

__attribute__((always_inline))
static inline void copy_pixel_8bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
}

__attribute__((always_inline))
static inline void copy_pixel_16bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
    *to++ = *from++;
}

__attribute__((always_inline))
static inline void copy_pixel_24bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
    *to++ = *from++;
    *to++ = *from++;
}

#define COPY_PIXEL_CODE_BLOCK(_bpp)                                                               \
    switch (usb_display->rotate_mask) {                                                          \
    case 0:                                                                                       \
    {                                                                                             \
        uint8_t *to = fb + (y_start * h_res + x_start) * bytes_per_pixel;                         \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            memcpy(to, from, copy_bytes_per_line);                                                \
            to += bytes_per_line;                                                                 \
            from += copy_bytes_per_line;                                                          \
        }                                                                                         \
    }                                                                                             \
    break;                                                                                        \
    case ROTATE_MASK_MIRROR_X:                                                                    \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            uint32_t index = (y * h_res + (h_res - 1 - x_start)) * bytes_per_pixel;               \
            for (size_t x = x_start; x < x_end; x++)                                              \
            {                                                                                     \
                copy_pixel_##_bpp##bpp(to + index, from);                                         \
                index -= bytes_per_pixel;                                                         \
                from += bytes_per_pixel;                                                          \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_MIRROR_Y:                                                                    \
    {                                                                                             \
        uint8_t *to = fb + ((v_res - 1 - y_start) * h_res + x_start) * bytes_per_pixel;           \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            memcpy(to, from, copy_bytes_per_line);                                                \
            to -= bytes_per_line;                                                                 \
            from += copy_bytes_per_line;                                                          \
        }                                                                                         \
    }                                                                                             \
    break;                                                                                        \
    case ROTATE_MASK_MIRROR_X | ROTATE_MASK_MIRROR_Y:                                             \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            uint32_t index = ((v_res - 1 - y) * h_res + (h_res - 1 - x_start)) * bytes_per_pixel; \
            for (size_t x = x_start; x < x_end; x++)                                              \
            {                                                                                     \
                copy_pixel_##_bpp##bpp(to + index, from);                                         \
                index -= bytes_per_pixel;                                                         \
                from += bytes_per_pixel;                                                          \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY:                                                                     \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = (x * h_res + y) * bytes_per_pixel;                                   \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY | ROTATE_MASK_MIRROR_X:                                              \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = (x * h_res + h_res - 1 - y) * bytes_per_pixel;                       \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY | ROTATE_MASK_MIRROR_Y:                                              \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = ((v_res - 1 - x) * h_res + y) * bytes_per_pixel;                     \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY | ROTATE_MASK_MIRROR_X | ROTATE_MASK_MIRROR_Y:                       \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = ((v_res - 1 - x) * h_res + h_res - 1 - y) * bytes_per_pixel;         \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    default:                                                                                      \
        break;                                                                                    \
    }

static esp_err_t panel_usb_display_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                               const void *color_data)
{
    ESP_RETURN_ON_FALSE((x_start < x_end) && (y_start < y_end), ESP_ERR_INVALID_ARG, TAG,
                        "start position must be smaller than end position");
    usb_display_t *usb_display = __containerof(panel, usb_display_t, base);

    // check if we need to copy the draw buffer (pointed by the color_data) to the driver's frame buffer
    bool do_copy = true;
    for (int i = 0; i < usb_display->fb_rgb_nums; i++) {
        if (color_data == usb_display->fb_rgb[i]) {
            usb_display->fb_rgb_index = i;
            do_copy = false;
            break;
        }
    }

    if (do_copy) {
        // adjust the flush window by adding extra gap
        x_start += usb_display->x_gap;
        y_start += usb_display->y_gap;
        x_end += usb_display->x_gap;
        y_end += usb_display->y_gap;
        // round the boundary
        int h_res = usb_display->h_res;
        int v_res = usb_display->v_res;
        if (usb_display->rotate_mask & ROTATE_MASK_SWAP_XY) {
            x_start = MIN(x_start, v_res);
            x_end = MIN(x_end, v_res);
            y_start = MIN(y_start, h_res);
            y_end = MIN(y_end, h_res);
        } else {
            x_start = MIN(x_start, h_res);
            x_end = MIN(x_end, h_res);
            y_start = MIN(y_start, v_res);
            y_end = MIN(y_end, v_res);
        }

        int bytes_per_pixel = usb_display->bytes_per_pixel;
        int pixels_per_line = usb_display->h_res;
        uint32_t bytes_per_line = bytes_per_pixel * pixels_per_line;
        uint8_t *fb = usb_display->fb_rgb[usb_display->fb_rgb_index];

        // copy the UI draw buffer into internal frame buffer
        const uint8_t *from = (const uint8_t *)color_data;
        uint32_t copy_bytes_per_line = (x_end - x_start) * bytes_per_pixel;
        size_t offset = y_start * copy_bytes_per_line + x_start * bytes_per_pixel;
        uint8_t *to = fb;
        if (1 == bytes_per_pixel) {
            COPY_PIXEL_CODE_BLOCK(8)
        } else if (2 == bytes_per_pixel) {
            COPY_PIXEL_CODE_BLOCK(16)
        } else if (3 == bytes_per_pixel) {
            COPY_PIXEL_CODE_BLOCK(24)
        }
    }

    return ESP_OK;
}

static esp_err_t panel_usb_display_reset(esp_lcd_panel_t *panel)
{
    ESP_LOGE(TAG, "reset is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_usb_display_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ESP_LOGE(TAG, "invert color is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_usb_display_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    usb_display_t *usb_display = __containerof(panel, usb_display_t, base);
    usb_display->rotate_mask &= ~(ROTATE_MASK_MIRROR_X | ROTATE_MASK_MIRROR_Y);
    usb_display->rotate_mask |= (mirror_x << USB_DISPLAY_MIRROR_X | mirror_y << USB_DISPLAY_MIRROR_Y);
    return ESP_OK;
}

static esp_err_t panel_usb_display_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    usb_display_t *usb_display = __containerof(panel, usb_display_t, base);
    usb_display->rotate_mask &= ~(ROTATE_MASK_SWAP_XY);
    usb_display->rotate_mask |= swap_axes << USB_DISPLAY_SWAP_XY;
    return ESP_OK;
}

static esp_err_t panel_usb_display_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    usb_display_t *usb_display = __containerof(panel, usb_display_t, base);
    usb_display->x_gap = x_gap;
    usb_display->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_usb_display_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ESP_LOGE(TAG, "display on/off is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_lcd_usb_display_register_event_callbacks(esp_lcd_panel_handle_t panel, esp_lcd_jpeg_buf_finish_cb_t *callbacks, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(panel && callbacks, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    usb_display_t *usb_display = __containerof(panel, usb_display_t, base);

    usb_display->on_jpeg_encode_done = callbacks;
    return ESP_OK;
}
