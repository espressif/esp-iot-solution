/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_xiaozhi_transport.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_XIAOZHI_MQTT_MAX_STRING_SIZE 256
#define ESP_XIAOZHI_MQTT_MAX_PAYLOAD_SIZE 2048
#define ESP_XIAOZHI_MQTT_DEFAULT_PORT 8883

typedef uint32_t esp_xiaozhi_mqtt_handle_t;

/**
 * @brief  Initialize the mqtt application
 *
 * @param[out] handle  Output MQTT handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle pointer
 *       - ESP_ERR_NO_MEM        Failed to allocate MQTT context
 */
esp_err_t esp_xiaozhi_mqtt_init(esp_xiaozhi_mqtt_handle_t *handle);

/**
 * @brief  Deinitialize the mqtt application
 *
 * @param[in] handle  MQTT handle from init
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_mqtt_deinit(esp_xiaozhi_mqtt_handle_t handle);

/**
 * @brief  Start the mqtt application
 *
 * @param[in] handle  MQTT handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - ESP_ERR_NOT_FOUND     MQTT configuration is missing (namespace or endpoint)
 *       - Other                 Error from keystore or esp_mqtt_client_init/start
 */
esp_err_t esp_xiaozhi_mqtt_start(esp_xiaozhi_mqtt_handle_t handle);

/**
 * @brief  Stop the mqtt application
 *
 * @param[in] handle  MQTT handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - Other                 Error from esp_mqtt_client_stop
 */
esp_err_t esp_xiaozhi_mqtt_stop(esp_xiaozhi_mqtt_handle_t handle);

/**
 * @brief  Send text to the mqtt application
 *
 * @param[in] handle  MQTT handle
 * @param[in] text    Null-terminated text to publish
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle, text, or publish topic not set; or publish failed
 */
esp_err_t esp_xiaozhi_mqtt_send_text(esp_xiaozhi_mqtt_handle_t handle, const char *text);

/**
 * @brief  Set the callbacks for the mqtt application
 *
 * @param[in] handle           MQTT handle
 * @param[in] transport_handle  Transport handle to pass to callbacks
 * @param[in] callbacks       Callbacks (must not be NULL)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or callbacks
 */
esp_err_t esp_xiaozhi_mqtt_set_callbacks(esp_xiaozhi_mqtt_handle_t handle, esp_xiaozhi_transport_handle_t transport_handle, const esp_xiaozhi_transport_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
