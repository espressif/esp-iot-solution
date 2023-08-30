/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_camera.h"
#include "camera_pin.h"

#ifdef CONFIG_FORMAT_MJPEG
#define UVC_FORMAT_JPEG     1
#endif

#ifdef CONFIG_CAMERA_MULTI_FRAMESIZE
//If enable, add VGA and HVGA to list
#define UVC_FRAME_MULTI     1
#endif

#define UVC_FRAME_BUF_SIZE  (CONFIG_CAMERA_DEFAULT_FRAMESIZE_SIZE * 1024)
#define UVC_FRAME_WIDTH     CONFIG_CAMERA_DEFAULT_FRAMESIZE_WIDTH
#define UVC_FRAME_HEIGHT    CONFIG_CAMERA_DEFAULT_FRAMESIZE_HEIGT
#define UVC_FRAME_RATE      CONFIG_CAMERA_DEFAULT_FRAMERATE
#define UVC_JPEG_QUALITY    CONFIG_CAMERA_QUALITY_JPEG

#ifdef CONFIG_FRAMESIZE_QVGA
#define UVC_FRAME_SIZE FRAMESIZE_QVGA
#endif
#ifdef CONFIG_FRAMESIZE_HVGA
#define UVC_FRAME_SIZE FRAMESIZE_HVGA
#endif
#ifdef CONFIG_FRAMESIZE_VGA
#define UVC_FRAME_SIZE FRAMESIZE_VGA
#endif
#ifdef CONFIG_FRAMESIZE_SVGA
#define UVC_FRAME_SIZE FRAMESIZE_SVGA
#endif
#ifdef CONFIG_FRAMESIZE_HD
#define UVC_FRAME_SIZE FRAMESIZE_HD
#endif
#ifdef CONFIG_FRAMESIZE_FHD
#define UVC_FRAME_SIZE FRAMESIZE_FHD
#endif

#ifdef CONFIG_UVC_MODE_BULK
#define UVC_BULK_MODE
#endif

static const struct {
    framesize_t frame_size;
    int width;
    int height;
    int rate;
    int jpeg_quality;
} UVC_FRAMES_INFO[] = {
    {UVC_FRAME_SIZE, UVC_FRAME_WIDTH, UVC_FRAME_HEIGHT, UVC_FRAME_RATE, UVC_JPEG_QUALITY},
    {FRAMESIZE_VGA, 640, 480, 15, 14},
    {FRAMESIZE_HVGA, 480, 320, 30, 10},
    {FRAMESIZE_QVGA, 320, 240, 30, 10},
};

#define UVC_FRAME_NUM (sizeof(UVC_FRAMES_INFO) / sizeof(UVC_FRAMES_INFO[0]))
_Static_assert(UVC_FRAME_NUM == 4, "UVC_FRAME_NUM must be 4");
