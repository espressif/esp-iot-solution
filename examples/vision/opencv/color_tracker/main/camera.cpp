/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include "esp_log.h"
#include "camera.hpp"

static const char *TAG = "cam";

Camera::Camera(const video_pix_fmt_t video_pix_fmt,
               const uint8_t fb_count,
               const v4l2_memory fb_mem_type,
               bool horizontal_flip) :
    m_video_pix_fmt(video_pix_fmt),
    m_fb_count(fb_count),
    m_fb_mem_type(fb_mem_type),
    m_horizontal_flip(horizontal_flip)
{
    m_mutex = xSemaphoreCreateMutex();
    m_cam_fbs = new cam_fb_t[m_fb_count];
    video_init();
}

Camera::~Camera()
{
    video_deinit();
    if (m_fb_mem_type == V4L2_MEMORY_USERPTR) {
        for (int i = 0; i < m_fb_count; i++) {
            heap_caps_free(m_cam_fbs[i].buf);
        }
    }
    delete [] m_cam_fbs;
}

esp_err_t Camera::open_video_device()
{
    struct v4l2_capability capability;
    m_fd = open("/dev/video0", O_RDONLY);
    if (m_fd < 0) {
        ESP_LOGE(TAG, "Failed to open device");
        return ESP_FAIL;
    }

    if (ioctl(m_fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "Failed to get capability");
        close(m_fd);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);

    return ESP_OK;
}

esp_err_t Camera::close_video_device()
{
    close(m_fd);
    return ESP_OK;
}

esp_err_t Camera::set_horizontal_flip()
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count = 1;
    controls.controls = control;
    control[0].id = V4L2_CID_HFLIP;
    control[0].value = 1;
    if (ioctl(m_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "Failed to mirror the frame horizontally");
        close(m_fd);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Camera::set_video_format()
{
    struct v4l2_format format;
    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "Failed to get format");
        close(m_fd);
        return ESP_FAIL;
    }
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = m_video_pix_fmt;
    m_width = format.fmt.pix.width;
    m_height = format.fmt.pix.height;
    if (ioctl(m_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "Failed to set format");
        close(m_fd);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Width=%" PRIu32 " Height=%" PRIu32, format.fmt.pix.width, format.fmt.pix.height);
    return ESP_OK;
}

esp_err_t Camera::start_video_stream()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start stream");
        close(m_fd);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Camera::stop_video_stream()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to stop stream");
        close(m_fd);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Camera::init_fbs()
{
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = m_fb_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = m_fb_mem_type;
    if (ioctl(m_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "Failed to require buffer");
        close(m_fd);
        return ESP_FAIL;
    }

    struct v4l2_buffer buf;
    size_t cache_line_size;
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, &cache_line_size));

    for (size_t i = 0; i < m_fb_count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = m_fb_mem_type;
        buf.index = i;

        if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "Failed to query buffer");
            close(m_fd);
            return ESP_FAIL;
        }

        cam_fb_t *fb = m_cam_fbs + buf.index;
        fb->len = buf.length;
        if (m_fb_mem_type == V4L2_MEMORY_MMAP) {
            fb->buf = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        } else {
            fb->buf = heap_caps_aligned_alloc(cache_line_size, buf.length, MALLOC_CAP_SPIRAM);
            buf.m.userptr = (unsigned long)fb->buf;
        }
        fb->width = m_width;
        fb->height = m_height;
        fb->format = m_video_pix_fmt;

        if (!fb->buf) {
            ESP_LOGE(TAG, "Failed to map/alloc buffer");
            close(m_fd);
            return ESP_FAIL;
        }

        if (ioctl(m_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "Failed to queue video frame");
            close(m_fd);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

void Camera::video_init()
{
    esp_video_init_csi_config_t csi_config[] = {
        {
            .sccb_config = {
                .init_sccb = true,
                .i2c_config = {
                    .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                    .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                    .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
                },
                .freq      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
            },
            .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
            .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
        },
    };
    esp_video_init_config_t cam_config{};
    cam_config.csi = csi_config;
    ESP_ERROR_CHECK(esp_video_init(&cam_config));
    ESP_ERROR_CHECK(open_video_device());
    if (m_horizontal_flip) {
        ESP_ERROR_CHECK(set_horizontal_flip());
    }
    ESP_ERROR_CHECK(set_video_format());
    ESP_ERROR_CHECK(init_fbs());
    ESP_ERROR_CHECK(start_video_stream());
}

void Camera::video_deinit()
{
    ESP_ERROR_CHECK(stop_video_stream());
    ESP_ERROR_CHECK(close_video_device());
}

cam_fb_t *Camera::cam_fb_get()
{
    if (m_buf_queue.size() + 1 >= m_fb_count) {
        ESP_LOGW(TAG, "Can not get more frame buffer.");
        return nullptr;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = m_fb_mem_type;
    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        return nullptr;
    }
    int64_t us = esp_timer_get_time();
    cam_fb_t *fb = m_cam_fbs + buf.index;
    fb->timestamp.tv_sec = us / 1000000UL;
    fb->timestamp.tv_usec = us % 1000000UL;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_v4l2_buf_queue.push(buf);
    m_buf_queue.push(fb);
    xSemaphoreGive(m_mutex);
    return fb;
}

cam_fb_t *Camera::cam_fb_peek(bool back)
{
    if (m_buf_queue.empty()) {
        ESP_LOGW(TAG, "Can not peek an empty frame buffer queue.");
        return nullptr;
    }
    if (back) {
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        cam_fb_t *fb = m_buf_queue.back();
        xSemaphoreGive(m_mutex);
        return fb;
    } else {
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        cam_fb_t *fb = m_buf_queue.front();
        xSemaphoreGive(m_mutex);
        return fb;
    }
}

void Camera::cam_fb_return()
{
    if (m_buf_queue.empty()) {
        ESP_LOGW(TAG, "Can not return more frame buffer.");
        return;
    }
    struct v4l2_buffer buf = m_v4l2_buf_queue.front();
    cam_fb_t *fb = m_cam_fbs + buf.index;
    if (m_fb_mem_type == V4L2_MEMORY_USERPTR) {
        buf.m.userptr = (unsigned long)fb->buf;
        buf.length = fb->len;
    }
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (ioctl(m_fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "Failed to queue video frame");
        return;
    }
    m_v4l2_buf_queue.pop();
    m_buf_queue.pop();
    xSemaphoreGive(m_mutex);
}
