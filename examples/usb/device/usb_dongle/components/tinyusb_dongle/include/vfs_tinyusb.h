/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_vfs_common.h" // For esp_line_endings_t definitions

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_TUSB_MAX_PATH 16
#define VFS_TUSB_PATH_DEFAULT "/dev/tusb_cdc"

/**
 * @brief Register TinyUSB CDC at VFS with path
 *
 * Know limitation:
 * In case there are multiple CDC interfaces in the system, only one of them can be registered to VFS.
 *
 * @param[in] cdc_intf Interface number of TinyUSB's CDC
 * @param[in] path     Path where the CDC will be registered, `/dev/tusb_cdc` will be used if left NULL.
 * @return esp_err_t ESP_OK or ESP_FAIL
 */
esp_err_t esp_vfs_tusb_cdc_register(int cdc_intf, char const *path);

/**
 * @brief Unregister TinyUSB CDC from VFS
 *
 * @param[in] path Path where the CDC will be unregistered if NULL will be used `/dev/tusb_cdc`
 * @return esp_err_t ESP_OK or ESP_FAIL
 */
esp_err_t esp_vfs_tusb_cdc_unregister(char const *path);

/**
 * @brief Set the line endings to sent
 *
 * This specifies the conversion between newlines ('\n', LF) on stdout and line
 * endings sent:
 *
 * - ESP_LINE_ENDINGS_CRLF: convert LF to CRLF
 * - ESP_LINE_ENDINGS_CR: convert LF to CR
 * - ESP_LINE_ENDINGS_LF: no modification
 *
 * @param[in] mode line endings to send
 */
void esp_vfs_tusb_cdc_set_tx_line_endings(esp_line_endings_t mode);

/**
 * @brief Set the line endings expected to be received
 *
 * This specifies the conversion between line endings received and
 * newlines ('\n', LF) passed into stdin:
 *
 * - ESP_LINE_ENDINGS_CRLF: convert CRLF to LF
 * - ESP_LINE_ENDINGS_CR: convert CR to LF
 * - ESP_LINE_ENDINGS_LF: no modification
 *
 * @param[in] mode line endings expected
 */
void esp_vfs_tusb_cdc_set_rx_line_endings(esp_line_endings_t mode);

#ifdef __cplusplus
}
#endif
