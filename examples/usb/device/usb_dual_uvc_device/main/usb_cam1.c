/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "avi_player.h"
#include "usb_cam.h"

#define WIDTH  CONFIG_UVC_CAM1_FRAMESIZE_WIDTH
#define HEIGHT CONFIG_UVC_CAM1_FRAMESIZE_HEIGT

static const char *TAG = "usb_cam1";
static uvc_fb_t s_fb;
static bool running = false;
static avi_player_handle_t avi_handle;

static void camera_stop_cb(void *cb_ctx)
{
    (void)cb_ctx;
    if (running) {
        avi_player_play_stop(avi_handle);
        running = false;
    }
    ESP_LOGI(TAG, "Camera:%"PRIu32" Stop", (uint32_t)cb_ctx);
}

static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
    (void)cb_ctx;
    ESP_LOGI(TAG, "Camera:%"PRIu32" Start", (uint32_t)cb_ctx);
    ESP_LOGI(TAG, "Format: %d, width: %d, height: %d, rate: %d", format, width, height, rate);

    running = true;
    avi_player_play_from_file(avi_handle, "/spiffs/p4_introduce.avi");

    return ESP_OK;
}

static uvc_fb_t* camera_fb_get_cb(void *cb_ctx)
{
    (void)cb_ctx;
    video_frame_info_t info;
    size_t buf_size = UVC_MAX_FRAMESIZE_SIZE;
    avi_player_get_video_buffer(avi_handle, (void **)&s_fb.buf, &buf_size, &info, pdMS_TO_TICKS(200));
    ESP_LOGD(TAG, "Camera buf_size: %d", buf_size);
    uint64_t us = (uint64_t)esp_timer_get_time();
    s_fb.timestamp.tv_sec = us / 1000000UL;
    s_fb.timestamp.tv_usec = us % 1000000UL;
    s_fb.len = buf_size;
    s_fb.width = info.width;
    s_fb.height = info.height;
#if CONFIG_FORMAT_MJPEG_CAM1
    s_fb.format = UVC_FORMAT_JPEG;
#else
    s_fb.format = UVC_FORMAT_H264;
#endif

    if (s_fb.len > UVC_MAX_FRAMESIZE_SIZE) {
        ESP_LOGE(TAG, "Frame size %d is larger than max frame size %d", s_fb.len, UVC_MAX_FRAMESIZE_SIZE);
        return NULL;
    }

    return &s_fb;
}

static void camera_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
{
    (void)cb_ctx;
    assert(fb == &s_fb);
}

static void avi_end_cb(void *arg)
{
    if (running) {
        avi_player_play_from_file(avi_handle, "/spiffs/p4_introduce.avi");
    }
}

esp_err_t usb_cam1_init(void)
{
    avi_player_config_t avi_cfg = {
        .avi_play_end_cb = avi_end_cb,
        .buffer_size = UVC_MAX_FRAMESIZE_SIZE,
    };

    avi_player_init(avi_cfg, &avi_handle);

    s_fb.buf = (uint8_t *)malloc(UVC_MAX_FRAMESIZE_SIZE);
    if (s_fb.buf == NULL) {
        ESP_LOGE(TAG, "malloc frame buffer fail");
        return ESP_FAIL;
    }

    uint32_t index = 1;
    uint8_t *uvc_buffer = (uint8_t *)malloc(UVC_MAX_FRAMESIZE_SIZE);
    if (uvc_buffer == NULL) {
        ESP_LOGE(TAG, "malloc frame buffer fail");
        return ESP_FAIL;
    }

    uvc_device_config_t config = {
        .uvc_buffer = uvc_buffer,
        .uvc_buffer_size = UVC_MAX_FRAMESIZE_SIZE,
        .start_cb = camera_start_cb,
        .fb_get_cb = camera_fb_get_cb,
        .fb_return_cb = camera_fb_return_cb,
        .stop_cb = camera_stop_cb,
        .cb_ctx = (void *)index,
    };

    ESP_LOGI(TAG, "Format List");
#if CONFIG_FORMAT_MJPEG_CAM1
    ESP_LOGI(TAG, "\tFormat(1) = %s", "MJPEG");
#else
    ESP_LOGI(TAG, "\tFormat(1) = %s", "H264");
#endif
    ESP_LOGI(TAG, "Frame List");
    ESP_LOGI(TAG, "\tFrame(1) = %d * %d @%dfps", UVC_FRAMES_INFO[index - 1][0].width, UVC_FRAMES_INFO[index - 1][0].height, UVC_FRAMES_INFO[index - 1][0].rate);
#if CONFIG_UVC_CAM1_MULTI_FRAMESIZE
    ESP_LOGI(TAG, "\tFrame(2) = %d * %d @%dfps", UVC_FRAMES_INFO[index - 1][1].width, UVC_FRAMES_INFO[index - 1][1].height, UVC_FRAMES_INFO[index - 1][1].rate);
    ESP_LOGI(TAG, "\tFrame(3) = %d * %d @%dfps", UVC_FRAMES_INFO[index - 1][2].width, UVC_FRAMES_INFO[index - 1][2].height, UVC_FRAMES_INFO[index - 1][2].rate);
    ESP_LOGI(TAG, "\tFrame(3) = %d * %d @%dfps", UVC_FRAMES_INFO[index - 1][3].width, UVC_FRAMES_INFO[index - 1][3].height, UVC_FRAMES_INFO[index - 1][3].rate);
#endif

    return uvc_device_config(index - 1, &config);
}
