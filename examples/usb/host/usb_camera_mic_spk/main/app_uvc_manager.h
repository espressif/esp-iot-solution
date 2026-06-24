/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"
#include "usb/uvc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_UVC_FORMAT_MJPEG = 0,
    APP_UVC_FORMAT_H264,
} app_uvc_format_t;

esp_err_t app_uvc_manager_start(void);
esp_err_t app_uvc_get_state_json(cJSON *root);
esp_err_t app_uvc_set_enabled(bool enabled);
esp_err_t app_uvc_set_format(app_uvc_format_t format, uint8_t resolution);
uvc_host_frame_t *app_uvc_get_frame(TickType_t wait_ticks);
esp_err_t app_uvc_return_frame(uvc_host_frame_t *frame);
bool app_uvc_is_streaming(void);
app_uvc_format_t app_uvc_get_format(void);

#ifdef __cplusplus
}
#endif
