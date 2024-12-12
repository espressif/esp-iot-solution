/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAMERA_PPA_BLEND_H_RES              (1024)
#define CAMERA_PPA_BLEND_V_RES              (600)

#define CAMERA_PPA_BLEND_TASK_STACK_SIZE    (8 * 1024)
#define CAMERA_PPA_BLEND_TASK_CORE_ID       (-1)
#define CAMERA_PPA_BLEND_TASK_PRIORITY      (2)

/**
 * @brief Initialize the camera PPA (Pixel Processing Accelerator) blend operation.
 *
 * This function initializes the necessary components for the camera PPA blend operation, including
 * I2C communication, video handling, and task creation.
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t camera_ppa_blend_init(void);

/**
 * @brief Notify the camera PPA blend operation that it is done.
 *
 */
void camera_ppa_blend_notify_done(void);

/**
 * @brief Switch on the camera stream.
 *
 */
void camera_stream_switch_on(void);

/**
 * @brief Switch off the camera stream.
 *
 */
void camera_stream_switch_off(void);

#ifdef __cplusplus
}
#endif
