/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <protocomm_ext_security.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROTOCOMM_EXT_TRANSPORT_METHOD_BLE,      /*!< BLE (NimBLE central) transport */
    PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,     /*!< HTTP/HTTPS transport */
    PROTOCOMM_EXT_TRANSPORT_METHOD_CONSOLE,  /*!< UART console transport */
    PROTOCOMM_EXT_TRANSPORT_METHOD_MAX,      /*!< Maximum transport method */
} protocomm_ext_transport_method_t;

typedef enum {
    PROTOCOMM_EXT_SECURITY_METHOD_NONE,        /*!< Security 0 (no encryption) */
    PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,  /*!< Security 1 (Curve25519 + AES-CTR) */
    PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2,  /*!< Security 2 (SRP-6a + AES-GCM) */
    PROTOCOMM_EXT_SECURITY_METHOD_MAX,         /*!< Maximum security method */
} protocomm_ext_security_method_t;

/**
 * @brief Configuration for creating a protocomm_ext instance
 *
 * transport_data / security_data meaning depends on the selected methods:
 * - HTTP:   transport_data -> esp_http_client_config_t *
 * - BLE:    transport_data -> NULL (scan/connect via NimBLE helpers)
 * - Console:transport_data -> protocomm_ext_console_config_t *
 * - Sec0:   security_data  -> NULL
 * - Sec1:   security_data  -> protocomm_ext_security1_params_t * (optional PoP)
 * - Sec2:   security_data  -> protocomm_ext_security2_params_t * (username/password)
 */
typedef struct {
    /**
     * Selected transport implementation.
     */
    protocomm_ext_transport_method_t transport_method;
    /**
     * Selected security scheme.
     */
    protocomm_ext_security_method_t security_method;
    /**
     * Transport-specific configuration pointer.
     *
     * Meaning depends on @c transport_method:
     * - HTTP: pointer to @c esp_http_client_config_t
     * - BLE: @c NULL
     * - Console: pointer to @c protocomm_ext_console_config_t
     */
    void *transport_data;
    /**
     * Security-specific configuration pointer.
     *
     * Meaning depends on @c security_method:
     * - Sec0: @c NULL
     * - Sec1: pointer to @c protocomm_ext_security1_params_t
     * - Sec2: pointer to @c protocomm_ext_security2_params_t
     */
    void *security_data;
} protocomm_ext_config_data_t;

/**
 * @brief Opaque protocomm_ext instance returned by protocomm_ext_new()
 */
typedef struct protocomm_ext protocomm_ext_t;

/**
 * @brief Create a new protocomm_ext instance
 *
 * @param[in] config Configuration data
 * @return
 *  - non-NULL : Success
 *  - NULL     : Invalid argument / unsupported method / OOM
 */
protocomm_ext_t *protocomm_ext_new(protocomm_ext_config_data_t *config);

/**
 * @brief Delete a protocomm_ext instance created by protocomm_ext_new()
 *
 * @param[in] pc Instance to delete (NULL-safe)
 */
void protocomm_ext_delete(protocomm_ext_t *pc);

/**
 * @brief Open a transport session
 *
 * @param[in] pc     Instance
 * @param[in] config Transport-specific connect config (BLE address, etc.; may be NULL for HTTP)
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_open_session(protocomm_ext_t *pc, const void *config);

/**
 * @brief Close a transport session opened by protocomm_ext_open_session()
 *
 * @param[in] pc Instance
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_close_session(protocomm_ext_t *pc);

/**
 * @brief Read version/capabilities from the peer version endpoint (plaintext)
 *
 * Typical endpoint names: "proto-ver", "esp_local_ctrl/version".
 * Caller must free *out_data on success.
 *
 * @param[in]  pc           Instance
 * @param[in]  ep_name      Version endpoint name
 * @param[out] out_data     Received payload
 * @param[out] out_data_len Received payload length
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_get_version_capabilities(protocomm_ext_t *pc, const char *ep_name,
                                                 uint8_t **out_data, size_t *out_data_len);

/**
 * @brief Change security scheme / credentials before the handshake
 *
 * Typical use: create with Sec0 → open transport → read proto-ver → call this
 * with the peer's sec_ver and user-entered PoP / username+password →
 * protocomm_ext_security_init() / establish_security().
 *
 * Cleans up any previous security instance and copied params.
 * Must be called before a successful establish_security().
 *
 * @param[in] pc             Instance
 * @param[in] security_method Sec0 / Sec1 / Sec2
 * @param[in] security_data  Same meaning as protocomm_ext_config_data_t::security_data
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_set_security(protocomm_ext_t *pc,
                                     protocomm_ext_security_method_t security_method,
                                     const void *security_data);

/**
 * @brief Locally prepare security state (e.g. Sec2 SRP client keygen)
 *
 * Does not talk to the peer. Call this before opening a SoftAP/HTTP session so
 * long CPU-bound work does not starve an idle keep-alive and get the STA kicked.
 *
 * @param[in] pc Instance
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_security_init(protocomm_ext_t *pc);

/**
 * @brief Run the selected security handshake on the session endpoint
 *
 * Typical endpoint names: "prov-session", "esp_local_ctrl/session".
 * Reuses an instance from protocomm_ext_security_init() when already prepared.
 *
 * @param[in] pc      Instance
 * @param[in] ep_name Session endpoint name
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_establish_security(protocomm_ext_t *pc, const char *ep_name);

/**
 * @brief Send application payload on a business endpoint (encrypt / decrypt as needed)
 *
 * This API is a pipe only: it does not encode/decode provisioning or local-ctrl
 * protobufs. Caller must free *out_data on success.
 *
 * @param[in]  pc           Instance
 * @param[in]  ep_name      Business endpoint name
 * @param[in]  data         Plaintext request
 * @param[in]  data_len     Request length
 * @param[out] out_data     Plaintext response
 * @param[out] out_data_len Response length
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_send_data(protocomm_ext_t *pc, const char *ep_name,
                                  const uint8_t *data, size_t data_len,
                                  uint8_t **out_data, size_t *out_data_len);

/**
 * @brief Get the underlying transport handle (e.g. for NimBLE scan helpers)
 *
 * @param[in] pc Instance
 * @return Transport handle, or NULL if pc is NULL / transport not initialized
 */
void *protocomm_ext_get_transport_handle(protocomm_ext_t *pc);

/**
 * @brief Transport selected when this instance was created
 *
 * @param[in] pc Instance
 * @return Transport method, or PROTOCOMM_EXT_TRANSPORT_METHOD_MAX if @p pc is NULL
 */
protocomm_ext_transport_method_t protocomm_ext_get_transport_method(const protocomm_ext_t *pc);

/**
 * @brief Register a logical endpoint name → BLE characteristic UUID16 mapping
 *
 * Required for BLE before get_version / establish_security / send_data that use
 * named endpoints. Mirrors device-side `wifi_prov_scheme_t::set_config_endpoint`
 * (e.g. manager.c registering "prov-session" → 0xFF51).
 *
 * Application components (network_provisioner, esp_local_controller, …) should
 * call this for each endpoint they use. HTTP and Console ignore @p uuid and
 * return ESP_OK.
 *
 * Re-registering the same name updates the UUID.
 *
 * @param[in] pc             Instance
 * @param[in] endpoint_name  Logical name (must not be NULL)
 * @param[in] uuid           16-bit UUID embedded in the peer GATT characteristic
 * @return ESP_OK on success
 */
esp_err_t protocomm_ext_set_config_endpoint(protocomm_ext_t *pc,
                                            const char *endpoint_name,
                                            uint16_t uuid);

#ifdef __cplusplus
}
#endif
