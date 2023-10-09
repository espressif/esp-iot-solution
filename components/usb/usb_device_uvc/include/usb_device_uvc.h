/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sys/time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UVC_FORMAT_JPEG,
} uvc_format_t;

typedef struct {
    uint8_t * buf;              /*!< Pointer to the pixel data */
    size_t len;                 /*!< Length of the buffer in bytes */
    size_t width;               /*!< Width of the buffer in pixels */
    size_t height;              /*!< Height of the buffer in pixels */
    uvc_format_t format;        /*!< Format of the pixel data */
    struct timeval timestamp;   /*!< Timestamp since boot of the first DMA buffer of the frame */
} uvc_fb_t;

typedef esp_err_t (*uvc_input_start_cb_t)(uvc_format_t format, int width, int height, int rate, void *cb_ctx);
typedef uvc_fb_t* (*uvc_input_fb_get_cb_t)(void *cb_ctx);
typedef void (*uvc_input_fb_return_cb_t)(uvc_fb_t *fb, void *cb_ctx);
typedef void (*uvc_input_stop_cb_t)(void *cb_ctx);

typedef struct {
    uint8_t *uvc_buffer;                   /*!< UVC transfer buffer */
    uint32_t uvc_buffer_size;              /*!< UVC transfer buffer size, should bigger than one frame size */
    uvc_input_start_cb_t start_cb;         /*!< callback function for UVC start */
    uvc_input_fb_get_cb_t fb_get_cb;       /*!< callback function for UVC get frame buffer */
    uvc_input_fb_return_cb_t fb_return_cb; /*!< callback function for UVC return frame buffer */
    uvc_input_stop_cb_t stop_cb;           /*!< callback function for UVC stop */
    void *cb_ctx;                          /*!< callback context */
} uvc_device_config_t;

/**
 * @brief Initialize the UVC device
 *
 * @param config Configuration for the UVC device
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if the configuration is invalid
 *         ESP_FAIL if the UVC device could not be initialized
 */
esp_err_t uvc_device_init(uvc_device_config_t *config);

/**
 * @brief Deinitialize the UVC device
 * TODO: This function is not implemented yet because tinyusb does not support deinitialization
 * @return ESP_OK on success
 *         ESP_FAIL if the UVC device could not be deinitialized
 */
//esp_err_t uvc_device_deinit(void);

#ifdef __cplusplus
}
#endif
