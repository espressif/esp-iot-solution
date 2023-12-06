/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef _MANAGER_H_
#define _MANAGER_H_

#ifdef CONFIG_OTA_WITH_PROTOCOMM
#include <protocomm.h>
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(ESP_BLE_OTA_EVENT);

typedef enum {
    OTA_INIT,
    OTA_DEINIT,
    OTA_END,
    OTA_FILE_RCV,
} esp_ble_ota_cb_event_t;

typedef void (*esp_ble_ota_cb_func_t)(void *user_data, esp_ble_ota_cb_event_t event, void *event_data);

/**
 * @brief   Event handler that is used by the manager while
 *          ota service is active
 */
typedef struct {
    /**
     * Callback function to be executed on ota events
     */
    esp_ble_ota_cb_func_t event_cb;

    /**
     * User context data to pass as parameter to callback function
     */
    void *user_data;
} esp_ble_ota_event_handler_t;

/**
 * @brief Event handler can be set to none if not used
 */
#define ESP_BLE_OTA_EVENT_HANDLER_NONE { \
    .event_cb  = NULL,                 \
    .user_data = NULL                  \
}

/**
 * @brief   Structure for specifying the ota scheme to be
 *          followed by the manager
 *
 * @note    Ready to use schemes are available:
 *              - esp_ble_ota_scheme_ble     : for ota over BLE transport + GATT server
 */
typedef struct esp_ble_ota_scheme {
    /**
     * Function which is to be called by the manager when it is to
     * start the ota service associated with a protocomm instance
     * and a scheme specific configuration
     */
    esp_err_t (*ota_start)(protocomm_t *pc, void *config);

    /**
     * Function which is to be called by the manager to stop the
     * ota service previously associated with a protocomm instance
     */
    esp_err_t (*ota_stop)(protocomm_t *pc);

    /**
     * Function which is to be called by the manager to generate
     * a new configuration for the ota service, that is
     * to be passed to ota_start()
     */
    void *(*new_config)(void);

    /**
     * Function which is to be called by the manager to delete a
     * configuration generated using new_config()
     */
    void (*delete_config)(void *config);

    /**
     * Function which is to be called by the manager to set the
     * service name and key values in the configuration structure
     */
    esp_err_t (*set_config_service)(void *config, const char *service_name, const char *service_key);

    /**
     * Function which is to be called by the manager to set a protocomm endpoint
     * with an identifying name and UUID in the configuration structure
     */
    esp_err_t (*set_config_endpoint)(void *config, const char *endpoint_name, uint16_t uuid);

} esp_ble_ota_scheme_t;

/**
 * @brief   Structure for specifying the manager configuration
 */
typedef struct {
    /**
     * Provisioning scheme to use. Following schemes are already available:
     *     - esp_ble_ota_scheme_ble     : for ota over BLE transport + GATT server
     */
    esp_ble_ota_scheme_t scheme;

    /**
     * Event handler required by the scheme for incorporating scheme specific
     * behavior while ota manager is running. Various options may be
     * provided by the scheme for setting this field. Use ESP_BLE_OTA_EVENT_HANDLER_NONE
     * when not used. When using scheme esp_ble_ota_scheme_ble, the following
     * options are available:
     *     - ESP_BLE_OTA_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
     *     - ESP_BLE_OTA_SCHEME_BLE_EVENT_HANDLER_FREE_BLE
     *     - ESP_BLE_OTA_SCHEME_BLE_EVENT_HANDLER_FREE_BT
     */
    esp_ble_ota_event_handler_t scheme_event_handler;

    /**
     * Event handler that can be set for the purpose of incorporating application
     * specific behavior. Use ESP_BLE_OTA_EVENT_HANDLER_NONE when not used.
     */
    esp_ble_ota_event_handler_t app_event_handler;
} esp_ble_ota_config_t;

/**
 * @brief   Security modes supported by the ota Manager.
 *
 * These are same as the security modes provided by protocomm
 */
typedef enum esp_ble_ota_security {
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_0
    /**
     * No security (plain-text communication)
     */
    ESP_BLE_OTA_SECURITY_0 = 0,
#endif
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1
    /**
     * This secure communication mode consists of
     *   X25519 key exchange
     * + proof of possession (pop) based authentication
     * + AES-CTR encryption
     */
    ESP_BLE_OTA_SECURITY_1,
#endif
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
    /**
     * This secure communication mode consists of
     *  SRP6a based authentication and key exchange
     *  + AES-GCM encryption/decryption
     */
    ESP_BLE_OTA_SECURITY_2
#endif
} esp_ble_ota_security_t;

/**
 * @brief  Security 1 params structure
 *         This needs to be passed when using ESP_BLE_OTA_SECURITY_1
 */
typedef const char esp_ble_ota_security1_params_t;

/**
 * @brief  Security 2 params structure
 *         This needs to be passed when using ESP_BLE_OTA_SECURITY_2
 */
typedef protocomm_security2_params_t esp_ble_ota_security2_params_t;

/**
 * @brief   Initialize ota manager instance
 *
 * Configures the manager and allocates internal resources
 *
 * Configuration specifies the ota scheme (transport)
 * and event handlers
 *
 * Event OTA_INIT is emitted right after initialization
 * is complete
 * @param[in] config Configuration structure
 *
 * @return
 *  - ESP_OK      : Success
 *  - ESP_FAIL    : Fail
 */
esp_err_t esp_ble_ota_init(esp_ble_ota_config_t config);

/**
 * @brief   Stop ota (if running) and release
 *          resource used by the manager
 *
 * Event OTA_DEINIT is emitted right after de-initialization
 * is finished
 *
 * If ota service is  still active when this API is called,
 * it first stops the service, hence emitting OTA_END, and
 * then performs the de-initialization
 */
void esp_ble_ota_deinit(void);

/**
 * @brief   Start ota service
 *
 * This starts the ota service according to the scheme
 * configured at the time of initialization. For scheme :
 * - ota_scheme_ble : This starts protocomm_ble, which internally initializes
 *                          BLE transport and starts GATT server for handling
 *                          ota requests
 *
 * Event OTA_START is emitted right after ota starts without failure
 *
 * @param[in] security      Specify which protocomm security scheme to use :
 *                              - OTA_SECURITY_0 : For no security
 *                              - OTA_SECURITY_1 : x25519 secure handshake for session
 *                                establishment followed by AES-CTR encryption of ota messages
 *                              - OTA_SECURITY_2:  SRP6a based authentication and key exchange
 *                                followed by AES-GCM encryption/decryption of ota messages
 * @param[in] ota_sec_params
 *                          Pointer to security params (NULL if not needed).
 *                          This is not needed for protocomm security 0
 *                          This pointer should hold the struct of type
 *                          ota_security1_params_t for protocomm security 1
 *                          and ota_security2_params_t for protocomm security 2 respectively.
 *                          This pointer and its contents should be valid till the ota service is
 *                          running and has not been stopped or de-inited.

 * @param[in] service_name  Unique name of the service. This translates to:
 *                              - Device name when ota mode is BLE

 * @param[in] service_key   Key required by client to access the service (NULL if not needed).
 *                          This translates to:
 *                              - ignored when ota mode is BLE
 *
 * @return
 *  - ESP_OK      : OTA started successfully
 *  - ESP_FAIL    : Failed to start ota service
 *  - ESP_ERR_INVALID_STATE : OTA manager not initialized or already started
 */
esp_err_t esp_ble_ota_start(esp_ble_ota_security_t security, const void *esp_ble_ota_sec_params, const char *service_name, const char *service_key);

/**
 * @brief   Stop ota service
 *
 * If ota service is active, this API will initiate a process to stop
 * the service and return. Once the service actually stops, the event OTA_END
 * will be emitted.
 *
 * If esp_ble_ota_deinit() is called without calling this API first, it will
 * automatically stop the ota service and emit the OTA_END, followed
 * by OTA_DEINIT, before returning.
 *
 * This API will generally be used in the scenario when the main application
 * has registered its own endpoints, and wishes that the ota service is stopped
 * only when some protocomm command from the client side application is received.
 *
 * Calling this API inside an endpoint handler, with sufficient cleanup_delay,
 * will allow the response / acknowledgment to be sent successfully before the
 * underlying protocomm service is stopped.
 *
 * For straightforward cases, using this API is usually not necessary as
 * ota is stopped automatically once OTA_CRED_SUCCESS is emitted.
 */
void esp_ble_ota_stop(void);

/**
 * @brief   Wait for ota service to finish
 *
 * Calling this API will block until ota service is stopped
 * i.e. till event OTA_END is emitted.
 *
 * This will not block if ota is not started or not initialized.
 */
void esp_ble_ota_wait(void);

/**
 * @brief   Create an additional endpoint and allocate internal resources for it
 *
 * This API is to be called by the application if it wants to create an additional
 * endpoint. All additional endpoints will be assigned UUIDs starting from 0xFF54
 * and so on in the order of execution.
 *
 * protocomm handler for the created endpoint is to be registered later using
 * esp_ble_ota_endpoint_register() after ota has started.
 *
 * @note    This API can only be called BEFORE ota is started
 *
 * @note    Additional endpoints can be used for configuring client provided
 *          parameters, that are necessary for the
 *          main application and hence must be set prior to starting the application
 *
 * @note    After session establishment, the additional endpoints must be targeted
 *          first by the client side application before sending OTA configuration,
 *          because once OTA configuration finishes the ota service is
 *          stopped and hence all endpoints are unregistered
 *
 * @param[in] ep_name unique name of the endpoint
 *
 * @return
 *  - ESP_OK      : Success
 *  - ESP_FAIL    : Failure
 */
esp_err_t esp_ble_ota_endpoint_create(const char *ep_name);

/**
 * @brief   Register a handler for the previously created endpoint
 *
 * This API can be called by the application to register a protocomm handler
 * to any endpoint that was created using esp_ble_ota_endpoint_create().
 *
 * @note    This API can only be called AFTER ota has started
 *
 * @note    Additional endpoints can be used for configuring client provided
 *          parameters, that are necessary for the
 *          main application and hence must be set prior to starting the application
 *
 * @note    After session establishment, the additional endpoints must be targeted
 *          first by the client side application before sending OTA configuration,
 *          because once OTA configuration finishes the ota service is
 *          stopped and hence all endpoints are unregistered
 *
 * @param[in] ep_name   Name of the endpoint
 * @param[in] handler   Endpoint handler function
 * @param[in] user_ctx  User data
 *
 * @return
 *  - ESP_OK      : Success
 *  - ESP_FAIL    : Failure
 */
esp_err_t esp_ble_ota_endpoint_register(const char *ep_name,
                                        protocomm_req_handler_t handler,
                                        void *user_ctx);

/**
 * @brief   Unregister the handler for an endpoint
 *
 * This API can be called if the application wants to selectively
 * unregister the handler of an endpoint while the OTA
 * is still in progress.
 *
 * All the endpoint handlers are unregistered automatically when
 * the OTA stops.
 *
 * @param[in] ep_name Name of the endpoint
 */
void esp_ble_ota_endpoint_unregister(const char *ep_name);

/**
 * @brief   Register the callback handler for writing data to partition
 *
 * This API is called by handlers once data is decrpyted and unpacked
 * and to be written on OTA partition.
 *
 * @param[in] buf       Data to be written on OTA partition
 * @param[in] length    Length of data to be written
 */
void ota_recv_fw_cb(uint8_t *buf, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_OTA_WITH_PROTOCOMM */
#endif
