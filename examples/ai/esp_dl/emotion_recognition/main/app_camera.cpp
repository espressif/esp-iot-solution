/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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
#include "esp_cache.h"
#include "bsp/esp-bsp.h"
#include "app_camera.hpp"

/* Per-board camera post-processing (byte-swap/rotation/mirror) done by the PPA.
 * All zero (e.g. ESP32-P4 MIPI-CSI) skips the PPA and uses frames zero-copy. */
#ifndef BSP_CAMERA_RGB565_BYTE_SWAP
#define BSP_CAMERA_RGB565_BYTE_SWAP (0)
#endif
#ifndef BSP_CAMERA_ROTATION_DEGREES
#define BSP_CAMERA_ROTATION_DEGREES (0)
#endif
#ifndef BSP_CAMERA_MIRROR_X
#define BSP_CAMERA_MIRROR_X (0)
#endif
#ifndef BSP_CAMERA_MIRROR_Y
#define BSP_CAMERA_MIRROR_Y (0)
#endif

#define CAM_USE_PPA (BSP_CAMERA_RGB565_BYTE_SWAP || BSP_CAMERA_ROTATION_DEGREES || \
                     BSP_CAMERA_MIRROR_X || BSP_CAMERA_MIRROR_Y)

static const char *TAG = "cam";

static inline ppa_srm_rotation_angle_t cam_ppa_rotation(void)
{
    switch (BSP_CAMERA_ROTATION_DEGREES) {
    case 90:  return PPA_SRM_ROTATION_ANGLE_90;
    case 180: return PPA_SRM_ROTATION_ANGLE_180;
    case 270: return PPA_SRM_ROTATION_ANGLE_270;
    default:  return PPA_SRM_ROTATION_ANGLE_0;
    }
}

Camera::Camera(const video_pix_fmt_t video_pix_fmt,
               const uint8_t fb_count,
               const v4l2_memory fb_mem_type,
               bool horizontal_flip) :
    m_video_pix_fmt(video_pix_fmt),
    m_fb_count(fb_count),
    m_fb_mem_type(fb_mem_type),
    m_horizontal_flip(horizontal_flip),
    m_use_ppa(CAM_USE_PPA),
    m_ppa_srm(nullptr),
    m_raw_bufs(nullptr),
    m_raw_buf_len(0),
    m_proc_bufs(nullptr),
    m_proc_buf_size(0),
    m_out_width(0),
    m_out_height(0)
{
    m_mutex = xSemaphoreCreateMutex();
    m_cam_fbs = new cam_fb_t[m_fb_count];
    m_raw_bufs = new void *[m_fb_count]();
    m_proc_bufs = new uint8_t *[m_fb_count]();
    video_init();
}

Camera::~Camera()
{
    video_deinit();
    if (m_use_ppa) {
        for (int i = 0; i < m_fb_count; i++) {
            if (m_proc_bufs[i]) {
                heap_caps_free(m_proc_bufs[i]);
            }
        }
        if (m_ppa_srm) {
            ppa_unregister_client(m_ppa_srm);
        }
    }
    if (m_fb_mem_type == V4L2_MEMORY_USERPTR) {
        for (int i = 0; i < m_fb_count; i++) {
            if (m_raw_bufs[i]) {
                heap_caps_free(m_raw_bufs[i]);
            }
        }
    }
    delete [] m_proc_bufs;
    delete [] m_raw_bufs;
    delete [] m_cam_fbs;
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
    }
}

esp_err_t Camera::init_ppa()
{
    ppa_client_config_t ppa_config = {};
    ppa_config.oper_type = PPA_OPERATION_SRM;
    ppa_config.max_pending_trans_num = 1;
    return ppa_register_client(&ppa_config, &m_ppa_srm);
}

esp_err_t Camera::process_frame_ppa(uint32_t index)
{
    ppa_srm_oper_config_t srm = {};
    srm.in.buffer = m_raw_bufs[index];
    srm.in.pic_w = m_width;
    srm.in.pic_h = m_height;
    srm.in.block_w = m_width;
    srm.in.block_h = m_height;
    srm.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    srm.out.buffer = m_proc_bufs[index];
    srm.out.buffer_size = m_proc_buf_size;
    srm.out.pic_w = m_out_width;
    srm.out.pic_h = m_out_height;
    srm.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    srm.rotation_angle = cam_ppa_rotation();
    srm.scale_x = 1.0f;
    srm.scale_y = 1.0f;
    srm.byte_swap = (BSP_CAMERA_RGB565_BYTE_SWAP != 0);
    srm.mirror_x = (BSP_CAMERA_MIRROR_X != 0);
    srm.mirror_y = (BSP_CAMERA_MIRROR_Y != 0);
    srm.mode = PPA_TRANS_MODE_BLOCKING;

    esp_err_t ret = ppa_do_scale_rotate_mirror(m_ppa_srm, &srm);
    if (ret != ESP_OK) {
        return ret;
    }
    /* PPA wrote the output via DMA; invalidate so the CPU reads fresh pixels. */
    esp_cache_msync(m_proc_bufs[index], m_proc_buf_size,
                    ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    return ESP_OK;
}

esp_err_t Camera::open_video_device()
{
    struct v4l2_capability capability;
    m_fd = open(BSP_CAMERA_DEVICE, O_RDONLY);
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
        void *raw_buf;
        if (m_fb_mem_type == V4L2_MEMORY_MMAP) {
            raw_buf = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        } else {
            raw_buf = heap_caps_aligned_alloc(cache_line_size, buf.length, MALLOC_CAP_SPIRAM);
            buf.m.userptr = (unsigned long)raw_buf;
        }
        if (!raw_buf) {
            ESP_LOGE(TAG, "Failed to map/alloc buffer");
            close(m_fd);
            return ESP_FAIL;
        }
        m_raw_bufs[buf.index] = raw_buf;
        m_raw_buf_len = buf.length;

        if (m_use_ppa) {
            /* PPA writes the processed frame into a dedicated SPIRAM buffer. */
            m_proc_bufs[buf.index] = (uint8_t *)heap_caps_aligned_calloc(
                                         cache_line_size, 1, m_proc_buf_size,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!m_proc_bufs[buf.index]) {
                ESP_LOGE(TAG, "Failed to alloc PPA buffer");
                close(m_fd);
                return ESP_FAIL;
            }
            fb->buf = m_proc_bufs[buf.index];
            fb->len = m_proc_buf_size;
            fb->width = m_out_width;
            fb->height = m_out_height;
        } else {
            fb->buf = raw_buf;
            fb->len = buf.length;
            fb->width = m_width;
            fb->height = m_height;
        }
        fb->format = m_video_pix_fmt;

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
    /* Board-agnostic: the board (CSI on P4, DVP on S31) brings up esp_video. */
    ESP_ERROR_CHECK(bsp_camera_start(nullptr));
    ESP_ERROR_CHECK(open_video_device());
    if (m_horizontal_flip) {
        ESP_ERROR_CHECK(set_horizontal_flip());
    }
    ESP_ERROR_CHECK(set_video_format());

    /* 90/270 degree rotation swaps width and height. */
    if (BSP_CAMERA_ROTATION_DEGREES == 90 || BSP_CAMERA_ROTATION_DEGREES == 270) {
        m_out_width = m_height;
        m_out_height = m_width;
    } else {
        m_out_width = m_width;
        m_out_height = m_height;
    }
    m_proc_buf_size = (size_t)m_out_width * m_out_height * 2; // RGB565

    if (m_use_ppa) {
        ESP_ERROR_CHECK(init_ppa());
        ESP_LOGI(TAG, "PPA enabled: byte_swap=%d rotate=%d mirror_x=%d mirror_y=%d",
                 BSP_CAMERA_RGB565_BYTE_SWAP, BSP_CAMERA_ROTATION_DEGREES,
                 BSP_CAMERA_MIRROR_X, BSP_CAMERA_MIRROR_Y);
    }

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
    if (m_use_ppa) {
        /* PPA byte-swap/rotate/mirror: raw buffer -> fb->buf (processed). */
        if (process_frame_ppa(buf.index) != ESP_OK) {
            ESP_LOGE(TAG, "PPA process failed");
            ioctl(m_fd, VIDIOC_QBUF, &buf);
            return nullptr;
        }
    } else {
        /* Invalidate cache so the CPU reads the DMA'd frame, not stale data. */
        esp_cache_msync(fb->buf, fb->len, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    }
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
    if (m_fb_mem_type == V4L2_MEMORY_USERPTR) {
        buf.m.userptr = (unsigned long)m_raw_bufs[buf.index];
        buf.length = m_raw_buf_len;
    }
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (ioctl(m_fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "Failed to queue video frame");
        xSemaphoreGive(m_mutex);
        return;
    }
    m_v4l2_buf_queue.pop();
    m_buf_queue.pop();
    xSemaphoreGive(m_mutex);
}
