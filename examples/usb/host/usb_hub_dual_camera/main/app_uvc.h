/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "usb/uvc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width;
    int height;
} uvc_resolution_t;

typedef struct {
    int index;
    bool if_streaming;
    int active_resolution;
    int resolution_count;
    uvc_resolution_t resolution[];
} uvc_dev_info_t;

esp_err_t app_uvc_control_dev_by_index(int index, bool if_open, int resolution_index);

esp_err_t app_uvc_get_dev_frame_info(int index, uvc_dev_info_t **dev_info);

void app_uvc_free_dev_frame_info(uvc_dev_info_t *dev_info);

esp_err_t app_uvc_get_connect_dev_num(int *num);

uvc_host_frame_t *app_uvc_get_frame_by_index(int index);

esp_err_t app_uvc_return_frame_by_index(int index, uvc_host_frame_t *frame);

esp_err_t app_uvc_init(void);

#ifdef __cplusplus
}
#endif
