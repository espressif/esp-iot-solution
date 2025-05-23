/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "esp_timer.h"
#include "driver/ppa.h"
#include "app_video.h"
#include "app_lcd.h"
#include "camera_ppa_blend.h"
#include "ppa_blend.h"

#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#define CAMERA_PPA_BLEND_WAIT_TIMEOUT_MS    (30)

static char *TAG = "camera_ppa_blend";

static bool csi_camera_switch = true;

static SemaphoreHandle_t ppa_blend_done = NULL;

static void camera_video_frame_operation(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len, void *user_data);

ppa_blend_in_layer_t ppa_bg_layer = {
    .buffer = NULL,
    .w = CAMERA_PPA_BLEND_H_RES,
    .h = CAMERA_PPA_BLEND_V_RES,
    .color_mode = PPA_BLEND_COLOR_MODE_RGB565,
    .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
};

ppa_blend_block_t ppa_block = {
    .bg_offset_x = 0,
    .bg_offset_y = 0,
    .fg_offset_x = 0,
    .fg_offset_y = 0,
    .out_offset_x = 0,
    .out_offset_y = 0,
    .w = CAMERA_PPA_BLEND_H_RES,
    .h = CAMERA_PPA_BLEND_V_RES,
    .update_type = PPA_BLEND_UPDATE_BG,
};

esp_err_t camera_ppa_blend_init(void)
{
    ppa_blend_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(ppa_blend_done != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create semaphores");

    i2c_master_bus_handle_t i2c_bus_handle = app_get_i2c_handle();
    esp_err_t ret = app_video_main(i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "video main init failed with error 0x%x", ret);
        return ret;
    }

    // Open the video device
    int video_cam_fd0 = app_video_open(EXAMPLE_CAM_DEV_PATH, APP_VIDEO_FMT_RGB565);
    if (video_cam_fd0 < 0) {
        ESP_LOGE(TAG, "video cam open failed");
    }

    ESP_ERROR_CHECK(app_video_set_bufs(video_cam_fd0, EXAMPLE_CAM_BUF_NUM, NULL)); // When setting the camera video buffer, it can be written as NULL to automatically allocate the buffer using mapping

    // Register the video frame operation callback
    ESP_ERROR_CHECK(app_video_register_frame_operation_cb(camera_video_frame_operation));

    // Start the camera stream task
    ESP_ERROR_CHECK(app_video_stream_task_start(video_cam_fd0, 0, NULL));

    return ESP_OK;
}

void camera_ppa_blend_notify_done(void)
{
    xSemaphoreGive(ppa_blend_done);
}

static void camera_video_frame_operation(uint8_t *camera_buf, uint8_t camera_buf_index, uint32_t camera_buf_hes, uint32_t camera_buf_ves, size_t camera_buf_len, void *user_data)
{
    if (csi_camera_switch) {
        // Set the background layer buffer
        ppa_bg_layer.buffer = (void*)camera_buf;
        ppa_blend_set_bg_layer(&ppa_bg_layer);

        // Start the PPA blend operation
        ppa_blend_run(&ppa_block, -1);

        xSemaphoreTake(ppa_blend_done, 0);
        xSemaphoreTake(ppa_blend_done, pdMS_TO_TICKS(CAMERA_PPA_BLEND_WAIT_TIMEOUT_MS));
    }
}

void camera_stream_switch_on(void)
{
    csi_camera_switch = true;
    ESP_LOGD(TAG, "Camera stream switched on");
}

void camera_stream_switch_off(void)
{
    csi_camera_switch = false;
    ESP_LOGD(TAG, "Camera stream switched off");
}
