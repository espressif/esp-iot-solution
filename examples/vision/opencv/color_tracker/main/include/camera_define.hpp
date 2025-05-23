/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "linux/videodev2.h"

/**
 * @brief Camera pixel format
 */
typedef enum {
    VIDEO_PIX_FMT_RAW8_BGGR = V4L2_PIX_FMT_SBGGR8,
    VIDEO_PIX_FMT_RGB565 = V4L2_PIX_FMT_RGB565,
    VIDEO_PIX_FMT_RGB888 = V4L2_PIX_FMT_RGB24,
    VIDEO_PIX_FMT_YUV420 = V4L2_PIX_FMT_YUV420,
    VIDEO_PIX_FMT_YUV422P = V4L2_PIX_FMT_YUV422P,
} video_pix_fmt_t;

/**
 * @brief Camera frame buffer
 */
typedef struct {
    void *buf;                 /*!< Buffer pointer */
    size_t len;                /*!< Buffer length */
    int width;                 /*!< Frame width */
    int height;                /*!< Frame height */
    video_pix_fmt_t format;    /*!< Frame format */
    struct timeval timestamp;  /*!< Frame timestamp */
} cam_fb_t;
