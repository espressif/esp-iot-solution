/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "usb/uvc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RESOLUTION 10

typedef struct {
    int index;
    bool if_streaming;
    int active_resolution;
    struct {
        int width;
        int height;
    } resolution[MAX_RESOLUTION];
    int resolution_count;
} uvc_dev_info_t;

esp_err_t app_uvc_control_dev_by_index(int index, bool if_open, int resolution_index);

esp_err_t app_uvc_get_dev_frame_info(int index, uvc_dev_info_t *dev_info);

esp_err_t app_uvc_get_connect_dev_num(int *num);

uvc_host_frame_t *app_uvc_get_frame_by_index(int index);

esp_err_t app_uvc_return_frame_by_index(int index, uvc_host_frame_t *frame);

esp_err_t app_uvc_init(void);

#ifdef __cplusplus
}
#endif
