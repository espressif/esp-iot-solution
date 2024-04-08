/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "usb_device_uvc.h"
#include "esp_timer.h"
#include "uvc_frame_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UVC_MAX_FRAMESIZE_SIZE     (80*1024)

extern const unsigned char jpeg_start[] asm("_binary_800_572_jpeg_start");
extern const unsigned char jpeg_end[]   asm("_binary_800_572_jpeg_end");

/**
 * @brief Init USB Camera 1
 *
 * @return
 *      ESP_OK on success
 *      ESP_FAIL on failure
 */
esp_err_t usb_cam1_init(void);

/**
 * @brief Init USB Camera 2
 *
 * @return
 *      ESP_OK on success
 *      ESP_FAIL on failure
 */
esp_err_t usb_cam2_init(void);

#ifdef __cplusplus
}
#endif
