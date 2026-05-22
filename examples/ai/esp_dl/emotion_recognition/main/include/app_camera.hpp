/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_cache.h"
#include "esp_timer.h"
#include "esp_video_init.h"
#include "esp_private/esp_cache_private.h"
#include "camera_define.hpp"

class Camera {
public:
    Camera(const video_pix_fmt_t video_pix_fmt,
           const uint8_t fb_count,
           const v4l2_memory fb_mem_type,
           bool horizontal_flip);
    ~Camera();

    cam_fb_t *cam_fb_get();
    cam_fb_t *cam_fb_peek(bool back);
    void cam_fb_return();

private:
    void video_init();
    void video_deinit();
    esp_err_t open_video_device();
    esp_err_t close_video_device();
    esp_err_t set_horizontal_flip();
    esp_err_t set_video_format();
    esp_err_t init_fbs();
    esp_err_t start_video_stream();
    esp_err_t stop_video_stream();

    SemaphoreHandle_t m_mutex;
    video_pix_fmt_t m_video_pix_fmt;
    int m_fd;
    uint8_t m_fb_count;
    v4l2_memory m_fb_mem_type;
    int m_width;
    int m_height;
    cam_fb_t *m_cam_fbs;
    std::queue<struct v4l2_buffer> m_v4l2_buf_queue;
    std::queue<cam_fb_t *> m_buf_queue;
    bool m_horizontal_flip;
};
