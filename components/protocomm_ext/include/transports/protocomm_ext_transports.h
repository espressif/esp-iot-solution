/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Handle to the transport layer instance
 */
typedef void * protocomm_ext_transport_handle_t;

/**
 * @brief   Protocomm Ext transport object structure.
 *
 * The member functions are used for implementing transport
 * protocomm sessions.
 */
typedef struct protocomm_ext_transport {

    /**
     * Function for initializing/allocating transport infrastructure
     * @param handle Transport handle
     * @param config Configuration data for the transport
     * @return ESP_OK on success, otherwise an error code
     */
    esp_err_t (*init)(protocomm_ext_transport_handle_t *handle, const void *config);

    /**
     * Function for deinitializing transport infrastructure
     * @param handle Transport handle
     * @return ESP_OK on success, otherwise an error code
     */
    esp_err_t (*deinit)(protocomm_ext_transport_handle_t handle);

    /**
     * Function for connecting to a service (e.g., HTTP server, BLE device)
     * @param handle Transport handle
     * @param config Configuration data for the transport
     * @return ESP_OK on success, otherwise an error code
     */
    esp_err_t (*connect)(protocomm_ext_transport_handle_t handle, const void *config);

    /**
     * Function for disconnecting from a service
     * @param handle Transport handle
     * @return ESP_OK on success, otherwise an error code
     */
    esp_err_t (*disconnect)(protocomm_ext_transport_handle_t handle);

    /**
    * Function for sending data and receiving response
    * @param handle Transport handle
    * @param ep_name Endpoint name
    * @param data Input data buffer
    * @param data_len Input data length
    * @param out_data Output data buffer (caller must free)
    * @param out_data_len Output data length
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*send_data)(protocomm_ext_transport_handle_t handle,
                           const char *ep_name,
                           const uint8_t *data, ssize_t data_len,
                           uint8_t **out_data, ssize_t *out_data_len);

    /**
     * Optional: map logical endpoint name → BLE characteristic UUID16.
     * HTTP / Console may leave this NULL (UUID is unused).
     * Same role as wifi_prov_scheme_t::set_config_endpoint on the device side.
     *
     * @param handle        Transport handle
     * @param endpoint_name Logical name (e.g. "prov-session", "esp_local_ctrl/control")
     * @param uuid          16-bit characteristic UUID
     * @return ESP_OK on success
     */
    esp_err_t (*set_config_endpoint)(protocomm_ext_transport_handle_t handle,
                                     const char *endpoint_name,
                                     uint16_t uuid);
} protocomm_ext_transport_t;

#ifdef __cplusplus
}
#endif
