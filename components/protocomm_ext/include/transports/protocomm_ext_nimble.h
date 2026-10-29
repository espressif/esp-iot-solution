/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include <stdint.h>
#include "esp_err.h"
#include "host/ble_hs.h"
#include "protocomm_ext_transports.h"

/**
 * @brief One BLE scan result entry returned by esp_protocomm_ext_nimble_get_scanned_device_info()
 *
 * On success, `name` and `mfg_data` (when non-NULL) are heap buffers owned by the
 * caller and must be freed with free(). Missing fields are left NULL / zero length.
 */
typedef struct {
    ble_addr_t addr;             /*!< Peer address for open_session() */
    uint8_t *name;               /*!< UTF-8 Local Name copy (caller frees), or NULL */
    uint8_t name_len;            /*!< Length of name in bytes (excluding added NUL) */
    uint8_t *mfg_data;           /*!< Manufacturer data copy (caller frees), or NULL */
    uint8_t mfg_data_len;        /*!< Length of mfg_data in bytes */
} protocomm_ext_nimble_scanned_device_info_t;

/**
 * @brief BLE scan configuration
 */
typedef struct {
    uint32_t scan_timeout_ms;    /*!< Active scan duration in milliseconds */
} protocomm_ext_nimble_scan_config_t;

/**
 * @brief Protocomm Ext NimBLE transport implementation (central / client role)
 */
extern const protocomm_ext_transport_t protocomm_ext_transport_nimble;

/**
 * @brief Return number of devices currently in the scan result list
 *
 * @param[in] handle Transport handle from protocomm_ext_get_transport_handle()
 * @return Device count, or 0 on invalid handle / uninitialized host
 */
int esp_protocomm_ext_nimble_get_scanned_device_count(protocomm_ext_transport_handle_t handle);

/**
 * @brief Copy one scan result into @p info
 *
 * @param[in]  handle Transport handle
 * @param[in]  index  Zero-based index in the scan list
 * @param[out] info   Output structure; on success caller must free info->name / info->mfg_data
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_INVALID_ARG: Bad handle / NULL info / host not ready
 *  - ESP_ERR_NOT_FOUND: Index out of range
 *  - ESP_ERR_NO_MEM: Allocation failure
 */
esp_err_t esp_protocomm_ext_nimble_get_scanned_device_info(protocomm_ext_transport_handle_t handle,
                                                           int index,
                                                           protocomm_ext_nimble_scanned_device_info_t *info);

/**
 * @brief Start an active GAP discovery (scan)
 *
 * @param[in] handle Transport handle
 * @param[in] config Scan configuration (must not be NULL)
 * @return ESP_OK on success, otherwise an error code
 */
esp_err_t esp_protocomm_ext_nimble_start_scan(protocomm_ext_transport_handle_t handle,
                                              const protocomm_ext_nimble_scan_config_t *config);

/**
 * @brief Stop an ongoing GAP discovery
 *
 * @param[in] handle Transport handle
 * @return ESP_OK on success, otherwise an error code
 */
esp_err_t esp_protocomm_ext_nimble_stop_scan(protocomm_ext_transport_handle_t handle);

/**
 * @brief Look up a characteristic by service + characteristic UUID on the connected peer
 *
 * @param[in] handle   Transport handle (must be connected)
 * @param[in] svc_uuid Service UUID
 * @param[in] chr_uuid Characteristic UUID
 * @return ESP_OK if the characteristic exists, otherwise an error code
 */
esp_err_t esp_protocomm_ext_nimble_find_chr_uuid(protocomm_ext_transport_handle_t handle,
                                                 const ble_uuid_t *svc_uuid,
                                                 const ble_uuid_t *chr_uuid);
#endif

#ifdef __cplusplus
}
#endif
