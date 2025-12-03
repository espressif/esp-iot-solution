/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "usb/uvc_host.h"

/**
 * @brief Initialize the USB camera
 *
 * @param width The width of the camera
 * @param height The height of the camera
 * @return esp_err_t ESP_OK on success, other error code on failure
 */
esp_err_t usb_camera_init(size_t width, size_t height);

/**
 * @brief Get the frame from the USB camera
 *
 * @return uvc_host_frame_t* The frame from the USB camera, NULL if no frame is available
 */
uvc_host_frame_t *usb_camera_get_frame(void);

/**
 * @brief Return the frame to the USB camera
 *
 * @param frame The frame to return
 * @return esp_err_t ESP_OK on success, other error code on failure
 */
esp_err_t usb_camera_return_frame(uvc_host_frame_t *frame);

/**
 * @brief Restart the USB camera stream
 *
 * @return esp_err_t ESP_OK on success, other error code on failure
 */
esp_err_t usb_camera_restart_stream(void);
