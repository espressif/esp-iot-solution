/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
 * @brief  Optional application adapters for HTTP bootstrap and activation
 *
 *         Strings and context are borrowed for the duration of a call. A NULL config
 *         uses saved wifi.ota_url or CONFIG_XIAOZHI_OTA_URL, the persisted device/client
 *         identity, generic board information, and activation v1. Board and signing
 *         implementations belong to the application. Configure capabilities separately
 *         through esp_xiaozhi_protocol_set_capabilities() before bootstrap.
 */
typedef struct {
    const char *url;          /*!< Bootstrap endpoint; activation appends /activate */
    const char *device_id;    /*!< Optional MAC identity override */
    const char *client_id;    /*!< Optional persisted UUID override */
    const char *user_agent;   /*!< Optional board-name/firmware-version */
    const char *language;     /*!< Optional locale, defaults to zh-CN */
    const char *serial_number; /*!< Optional printable, nonblank serial (1-32 bytes) for v2 */
    esp_err_t (*board_json)(void *ctx, char *buffer, size_t capacity); /*!< Complete NUL-terminated system JSON */
    esp_err_t (*sign_challenge)(void *ctx, const uint8_t *challenge, size_t length, uint8_t hmac[32]); /*!< v2 HMAC-SHA256 provider */
    void (*on_response)(void *ctx, const char *json, size_t length); /*!< Borrowed complete bootstrap response */
    void *ctx;                /*!< User context passed to board_json, sign_challenge, and on_response */
} esp_xiaozhi_chat_bootstrap_config_t;

/**
 * @brief  Bootstrap Xiaozhi chat configuration using optional application adapters
 *
 * @param[in,out] info    Pointer to the information structure to populate
 * @param[in]     config  Optional bootstrap adapters and identity overrides (NULL uses defaults)
 *
 * @return
 *       - ESP_OK                    On success
 *       - ESP_ERR_INVALID_ARG       Invalid @p info pointer or invalid adapter output
 *       - ESP_ERR_NO_MEM            Failed to allocate working buffers or HTTP resources
 *       - ESP_ERR_INVALID_RESPONSE  Server response is malformed or missing a valid body
 *       - Other                     Error from board info, HTTP client, JSON parsing, or keystore
 */
esp_err_t esp_xiaozhi_chat_get_info_with_config(esp_xiaozhi_chat_info_t *info,
                                                const esp_xiaozhi_chat_bootstrap_config_t *config);

/**
 * @brief  Complete or check device activation with the same identity and endpoint as bootstrap
 *
 *         Activation v1 posts `{}`. Activation v2 requires @c sign_challenge; signing errors
 *         are propagated and never downgraded to v1. HTTP 202 returns @c ESP_ERR_TIMEOUT
 *         (pending); HTTP 200 returns @c ESP_OK. The application owns the retry policy.
 *
 * @param[in] info    Information structure returned by bootstrap (provides activation challenge)
 * @param[in] config  Optional identity/signing adapters matching bootstrap (NULL for v1)
 *
 * @return
 *       - ESP_OK                    Device activated
 *       - ESP_ERR_INVALID_ARG       Invalid @p info pointer or invalid serial_number
 *       - ESP_ERR_NOT_SUPPORTED     v2 activation requested without @c sign_challenge
 *       - ESP_ERR_TIMEOUT           Activation still pending (HTTP 202)
 *       - ESP_ERR_NO_MEM            Out of memory
 *       - ESP_ERR_INVALID_RESPONSE  Unexpected HTTP status or malformed response
 *       - Other                     Error from signing callback or HTTP client
 */
esp_err_t esp_xiaozhi_chat_activate(const esp_xiaozhi_chat_info_t *info,
                                    const esp_xiaozhi_chat_bootstrap_config_t *config);

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
