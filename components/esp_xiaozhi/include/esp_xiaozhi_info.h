/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Information for Xiaozhi chat
 */
typedef struct {
    char *current_version;          /*!< Current version of the firmware */
    char *firmware_version;         /*!< Firmware version */
    char *firmware_url;             /*!< Firmware URL */
    char *serial_number;            /*!< Serial number */
    char *activation_code;          /*!< Activation code */
    char *activation_challenge;     /*!< Activation challenge */
    char *activation_message;       /*!< Activation message */
    int activation_timeout_ms;      /*!< Activation timeout in milliseconds */
    bool has_serial_number;         /*!< Has serial number */
    bool has_new_version;           /*!< Has new version */
    bool has_activation_code;       /*!< Has activation code */
    bool has_activation_challenge;  /*!< Has activation challenge */
    bool has_mqtt_config;           /*!< Has MQTT config */
    bool has_websocket_config;      /*!< Has WebSocket config */
    bool has_server_time;           /*!< Has server time */
} esp_xiaozhi_chat_info_t;

/**
 * @brief  Get Xiaozhi Chat Information from the HTTP server
 *
 * The function posts board information to the configured service endpoint, parses the
 * response, updates the output structure, and persists MQTT/WebSocket settings to NVS
 * when present in the server response.
 *
 * @param[in,out] info  Pointer to the information structure
 *
 * @return
 *       - ESP_OK                  On success
 *       - ESP_ERR_INVALID_ARG     Invalid info pointer
 *       - ESP_ERR_NO_MEM          Failed to allocate working buffers or HTTP resources
 *       - ESP_ERR_INVALID_RESPONSE Server response is malformed or missing a valid body
 *       - Other                   Error from board info collection, HTTP client, JSON parsing,
 *                                 or keystore persistence
 */
esp_err_t esp_xiaozhi_chat_get_info(esp_xiaozhi_chat_info_t *info);

/**
 * @brief  Free Xiaozhi Chat Information
 *
 * Releases dynamically allocated string fields owned by @p info.
 * This function does not zero the full structure or reset the boolean flags.
 *
 * @param[in,out] info  Pointer to the information structure
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid info pointer
 */
esp_err_t esp_xiaozhi_chat_free_info(esp_xiaozhi_chat_info_t *info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
