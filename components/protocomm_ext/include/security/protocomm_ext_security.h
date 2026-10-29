/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Protocomm Ext Security 1 parameters: Proof Of Possession
 */
typedef struct protocomm_ext_security1_params {
    /**
     * Pointer to buffer containing the proof of possession data
     */
    const uint8_t *data;

    /**
     * Length (in bytes) of the proof of possession data
     */
    uint16_t len;
} protocomm_ext_security1_params_t;

/**
 * @brief   Protocomm Ext Security 2 parameters: username + password
 *
 * Strings need not be null-terminated when lengths are provided.
 */
typedef struct protocomm_ext_security2_params {
    /**
     * Username buffer used by the Security 2 SRP client.
     */
    const char *username;
    /**
     * Username length in bytes.
     */
    uint16_t username_len;
    /**
     * Password buffer used by the Security 2 SRP client.
     */
    const char *password;
    /**
     * Password length in bytes.
     */
    uint16_t password_len;
} protocomm_ext_security2_params_t;

typedef void * protocomm_ext_security_handle_t;

/**
 * @brief   Protocomm Ext security object structure.
 *
 * The member functions are used for implementing secure
 * protocomm sessions.
 *
 * @note    This structure should not have any dynamic
 *          members to allow re-entrancy
 */
typedef struct protocomm_ext_security {
    /**
    * Unique version number of security implementation
    */
    int ver;

    /**
    * Function for initializing/allocating security
    * infrastructure
    * @param handle Security handle
    * @param sec_params Security parameters
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*init)(protocomm_ext_security_handle_t *handle, const void *sec_params);

    /**
    * Function for deallocating security infrastructure
    * @param handle Security handle
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*cleanup)(protocomm_ext_security_handle_t handle);

    /**
    * Function for getting security send command0 data
    * @param handle Security handle
    * @param outbuf Output buffer, need to be freed by the caller
    * @param outlen Output length
    * @param priv_data Private data
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*security_send_command0)(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data);

    /**
    * Function for parsing security command0 response
    * @param handle Security handle
    * @param inbuf Input buffer
    * @param inlen Input length
    * @param priv_data Private data
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*security_parse_command0)(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data);

    /**
    * Function for getting security send command1 data
    * @param handle Security handle
    * @param outbuf Output buffer, need to be freed by the caller
    * @param outlen Output length
    * @param priv_data Private data
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*security_send_command1)(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data);

    /**
    * Function for parsing security command1 response
    * @param handle Security handle
    * @param inbuf Input buffer
    * @param inlen Input length
    * @param priv_data Private data
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*security_parse_command1)(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data);

    /**
    * Function which implements the encryption algorithm
    * @param handle Security handle
    * @param inbuf Input buffer
    * @param inlen Input length
    * @param outbuf Output buffer, need to be freed by the caller
    * @param outlen Output length
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*encrypt)(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen);

    /**
    * Function which implements the decryption algorithm
    * @param handle Security handle
    * @param inbuf Input buffer
    * @param inlen Input length
    * @param outbuf Output buffer, need to be freed by the caller
    * @param outlen Output length
    * @return ESP_OK on success, otherwise an error code
    */
    esp_err_t (*decrypt)(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen);
} protocomm_ext_security_t;

#ifdef __cplusplus
}
#endif
