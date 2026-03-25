/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Handle for the camera explain client
 */
typedef struct esp_xiaozhi_camera_ctx esp_xiaozhi_camera_handle_t;

/**
 * @brief  JPEG image provided by the application
 */
typedef struct {
    const uint8_t *data; /*!< JPEG buffer provided by the application */
    size_t len;          /*!< JPEG length in bytes */
} esp_xiaozhi_camera_frame_t;

/**
 * @brief  Configuration for the camera explain client
 */
typedef struct {
    const char *explain_url;   /*!< Explain service URL */
    const char *explain_token; /*!< Optional bearer token */
} esp_xiaozhi_camera_config_t;

/**
 * @brief  Create a camera explain client handle
 *
 * This API does not initialize or control the camera. The application is
 * responsible for capturing frames and passing them to
 * `esp_xiaozhi_camera_explain()`.
 *
 * @param[in]   config      Pointer to the explain client configuration, can be NULL
 * @param[out]  out_handle  Pointer to the output handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - ESP_ERR_INVALID_ARG  Invalid output handle
 */
esp_err_t esp_xiaozhi_camera_create(const esp_xiaozhi_camera_config_t *config, esp_xiaozhi_camera_handle_t **out_handle);

/**
 * @brief  Destroy the camera explain client handle
 *
 * @param[in]  handle  Handle to the explain client
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t esp_xiaozhi_camera_destroy(esp_xiaozhi_camera_handle_t *handle);

/**
 * @brief  Configure the vision explain endpoint and optional bearer token
 *
 * @param[in]  handle  Handle to the explain client
 * @param[in]  url     Explain service URL, or NULL to clear it
 * @param[in]  token   Optional bearer token, or NULL to clear it
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t esp_xiaozhi_camera_set_explain_url(esp_xiaozhi_camera_handle_t *handle, const char *url, const char *token);

/**
 * @brief  Upload an application-provided frame to the explain service as JPEG
 *
 * @param[in]   handle    Handle to the explain client
 * @param[in]   frame     JPEG image prepared by the application
 * @param[in]   question  Multipart "question" field sent to the server
 * @param[out]  out_buf   Optional response buffer
 * @param[in]   buf_size  Size of out_buf in bytes
 * @param[out]  out_len   Optional response length written to out_buf
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_INVALID_STATE Explain URL is not configured
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - Other                Error from HTTP upload
 */
esp_err_t esp_xiaozhi_camera_explain(esp_xiaozhi_camera_handle_t *handle,
                                     const esp_xiaozhi_camera_frame_t *frame,
                                     const char *question,
                                     char *out_buf, size_t buf_size, size_t *out_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
