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

#define ESP_XIAOZHI_VISION_EXPLAIN_URL_LEN   256
#define ESP_XIAOZHI_VISION_EXPLAIN_TOKEN_LEN 128

/**
 * @brief  Shared context for vision explain clients
 */
typedef struct {
    char explain_url[ESP_XIAOZHI_VISION_EXPLAIN_URL_LEN];
    char explain_token[ESP_XIAOZHI_VISION_EXPLAIN_TOKEN_LEN];
} esp_xiaozhi_vision_t;

/**
 * @brief  Initialize a shared vision context
 *
 * @param[out] vision  Vision context
 * @param[in]  url     Optional explain URL
 * @param[in]  token   Optional bearer token
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid vision pointer
 */
esp_err_t esp_xiaozhi_vision_init(esp_xiaozhi_vision_t *vision,
                                  const char *url,
                                  const char *token);

/**
 * @brief  Update explain URL and bearer token
 *
 * @param[in,out] vision  Vision context
 * @param[in]     url     Optional explain URL, NULL clears it
 * @param[in]     token   Optional bearer token, NULL clears it
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid vision pointer
 */
esp_err_t esp_xiaozhi_vision_set_endpoint(esp_xiaozhi_vision_t *vision,
                                          const char *url,
                                          const char *token);

/**
 * @brief  Send JPEG data to the explain endpoint
 *
 * @param[in]   vision     Vision context
 * @param[in]   log_tag    Tag used for logging and argument checks
 * @param[in]   filename   Multipart filename
 * @param[in]   question   Multipart question field
 * @param[in]   jpeg_data  JPEG payload
 * @param[in]   jpeg_len   JPEG payload length
 * @param[out]  out_buf    Optional response buffer
 * @param[in]   buf_size   Response buffer size
 * @param[out]  out_len    Optional response length
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_INVALID_STATE Explain URL is not configured
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - Other                Error from HTTP upload
 */
esp_err_t esp_xiaozhi_vision_explain_jpeg(const esp_xiaozhi_vision_t *vision,
                                          const char *log_tag,
                                          const char *filename,
                                          const char *question,
                                          const uint8_t *jpeg_data,
                                          size_t jpeg_len,
                                          char *out_buf,
                                          size_t buf_size,
                                          size_t *out_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
