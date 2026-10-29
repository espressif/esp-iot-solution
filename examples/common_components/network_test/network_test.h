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

/**
 * @brief Start network test
 */
esp_err_t start_ping_timer(void);

/**
 * @brief Stop network test
 */
esp_err_t stop_ping_timer(void);

/**
 * @brief Query public IP address via HTTP
 */
esp_err_t test_query_public_ip(void);

/**
 * @brief Simple to test download speed
 *
 * @param url URL to download from (HTTP or HTTPS)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t test_download_speed(const char *url);

/**
 * @brief Simple to test upload speed
 *
 * @param url URL to upload to (HTTP or HTTPS)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t test_upload_speed(const char *url);

#ifdef __cplusplus
}
#endif
