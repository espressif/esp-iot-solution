/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "linux/videodev2.h"

typedef enum {
    VIDEO_PIX_FMT_RAW8_BGGR = V4L2_PIX_FMT_SBGGR8,
    VIDEO_PIX_FMT_RGB565 = V4L2_PIX_FMT_RGB565,
    VIDEO_PIX_FMT_RGB888 = V4L2_PIX_FMT_RGB24,
    VIDEO_PIX_FMT_YUV420 = V4L2_PIX_FMT_YUV420,
    VIDEO_PIX_FMT_YUV422P = V4L2_PIX_FMT_YUV422P,
} video_pix_fmt_t;

typedef struct {
    void *buf;
    size_t len;
    int width;
    int height;
    video_pix_fmt_t format;
    struct timeval timestamp;
} cam_fb_t;
