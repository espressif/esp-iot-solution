/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "sdkconfig.h"

#ifdef CONFIG_FORMAT_MJPEG
#define FORMAT_MJPEG     1
#endif

#ifdef CONFIG_UVC_MULTI_FRAMESIZE
//If enable, add VGA and HVGA to list
#define UVC_FRAME_MULTI     1
#endif

#define UVC_FRAME_WIDTH     CONFIG_UVC_DEFAULT_FRAMESIZE_WIDTH
#define UVC_FRAME_HEIGHT    CONFIG_UVC_DEFAULT_FRAMESIZE_HEIGT
#define UVC_FRAME_RATE      CONFIG_UVC_DEFAULT_FRAMERATE

#ifdef CONFIG_UVC_MODE_BULK
#define UVC_BULK_MODE
#endif

static const struct {
    int width;
    int height;
    int rate;
} UVC_FRAMES_INFO[] = {
    {UVC_FRAME_WIDTH, UVC_FRAME_HEIGHT, UVC_FRAME_RATE},
    {640, 480, 15},
    {480, 320, 30},
    {320, 240, 30},
};

#define UVC_FRAME_NUM (sizeof(UVC_FRAMES_INFO) / sizeof(UVC_FRAMES_INFO[0]))
_Static_assert(UVC_FRAME_NUM == 4, "UVC_FRAME_NUM must be 4");
