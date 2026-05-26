/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "usb/uvc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t width;
    uint16_t height;
} app_uvc_frame_size_t;

typedef void (*app_uvc_frame_cb_t)(const uvc_host_frame_t *frame, void *user_ctx);

typedef struct {
    uint16_t max_width;
    uint16_t max_height;
    app_uvc_frame_cb_t frame_cb;
    void *user_ctx;
} app_uvc_config_t;

esp_err_t app_uvc_init(const app_uvc_config_t *config);

esp_err_t app_uvc_start(const app_uvc_frame_size_t *preferred_frame_size, app_uvc_frame_size_t *current_frame_size);

esp_err_t app_uvc_switch_resolution(app_uvc_frame_size_t *current_frame_size);

#ifdef __cplusplus
}
#endif
