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
#include "esp_event_base.h"
#include "esp_ble_conn_mgr.h"
#include "esp_ots.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/* BLE OTP EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_OTP_EVENTS);
/** @endcond **/

/* OTP PSM - Protocol/Service Multiplexer for Object Transfer Channel */
#define BLE_OTP_PSM_DEFAULT                                0x0025    /*!< Default PSM for Object Transfer Channel (Bluetooth SIG assigned) */

/* OTP L2CAP CoC MTU defaults */
#define BLE_OTP_L2CAP_COC_MTU_DEFAULT                      512       /*!< Default L2CAP CoC MTU for object transfer */
#define BLE_OTP_L2CAP_COC_MTU_MIN                         23        /*!< Minimum L2CAP CoC MTU */
#define BLE_OTP_EVENT_DATA_MAX_LEN                        255       /*!< Max length of data copy in TRANSFER_DATA_RECEIVED event (avoids stack overflow in handler task) */
#define BLE_OTP_OBJECT_SIZE_UNKNOWN                       UINT32_MAX  /*!< Object size unknown until metadata is received */

/* OTP Event IDs */
#define BLE_OTP_EVENT_OTS_DISCOVERED                       0x0001    /*!< OTS service discovered */
#define BLE_OTP_EVENT_OTS_DISCOVERY_FAILED                 0x0002    /*!< OTS service discovery failed */
#define BLE_OTP_EVENT_OBJECT_SELECTED                       0x0003    /*!< Object selected (via OLCP) */
#define BLE_OTP_EVENT_OBJECT_CHANGED                        0x0004    /*!< Object changed notification received */
#define BLE_OTP_EVENT_OACP_STARTED                          0x0005    /*!< OACP procedure started */
#define BLE_OTP_EVENT_OACP_ABORTED                          0x0006    /*!< OACP procedure aborted */
#define BLE_OTP_EVENT_OACP_RESPONSE                         0x0007    /*!< OACP response received (rsp_code e.g. Success, Invalid Object / object not found, etc.) */
#define BLE_OTP_EVENT_OLCP_RESPONSE                         0x0008    /*!< OLCP response received */
#define BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED            0x0009    /*!< Object Transfer Channel (L2CAP CoC) connected */
#define BLE_OTP_EVENT_TRANSFER_CHANNEL_DISCONNECTED         0x000A    /*!< Object Transfer Channel (L2CAP CoC) disconnected */
#define BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED                0x000B    /*!< Object transfer data received */
#define BLE_OTP_EVENT_TRANSFER_DATA_SENT                    0x000C    /*!< Object transfer data sent */
#define BLE_OTP_EVENT_TRANSFER_EOF                          0x000D    /*!< Object content fully received/sent */
#define BLE_OTP_EVENT_TRANSFER_COMPLETE                     0x000E    /*!< Read/Write completed after EOF and success */
#define BLE_OTP_EVENT_TRANSFER_ERROR                        0x000F    /*!< Object transfer error */

/**
 * @brief OTP Role
 */
typedef enum {
    BLE_OTP_ROLE_CLIENT = 0,                                /*!< OTP Client role */
    BLE_OTP_ROLE_SERVER,                                    /*!< OTP Server role */
} esp_ble_otp_role_t;

/**
 * @brief OTP Configuration
 */
typedef struct {
    esp_ble_otp_role_t role;                                /*!< OTP role (Client or Server) */
    uint16_t psm;                                           /*!< PSM for Object Transfer Channel (default: BLE_OTP_PSM_DEFAULT) */
    uint16_t l2cap_coc_mtu;                                /*!< L2CAP CoC MTU for object transfer (default: BLE_OTP_L2CAP_COC_MTU_DEFAULT) */
    bool auto_discover_ots;                                 /*!< Auto discover OTS service when connected (Client only, default: true) */
} esp_ble_otp_config_t;

/**
 * @brief OTP Object Information
 */
typedef struct {
    esp_ble_ots_id_t object_id;                            /*!< Object ID */
    uint8_t object_name[BLE_OTS_ATT_VAL_LEN];              /*!< Object name */
    uint16_t object_type;                                   /*!< Object type */
    esp_ble_ots_size_t object_size;                        /*!< Object size */
    esp_ble_ots_prop_t object_prop;                        /*!< Object properties */
    esp_ble_ots_utc_t first_created;                       /*!< First created time (if available) */
    esp_ble_ots_utc_t last_modified;                      /*!< Last modified time (if available) */
} esp_ble_otp_object_info_t;

/**
 * @brief OTP OACP Response Data
 *
 * @note rsp_code uses BLE_OTS_OACP_RSP_* (e.g. Success, Invalid Object for object not found/deleted).
 */
typedef struct {
    uint8_t req_op_code;                                    /*!< Request op code */
    uint8_t rsp_code;                                       /*!< Response code (BLE_OTS_OACP_RSP_*) */
    uint8_t rsp_parameter[20];                             /*!< Response parameter */
} esp_ble_otp_oacp_rsp_t;

/**
 * @brief OTP OLCP Response Data
 */
typedef struct {
    uint8_t req_op_code;                                    /*!< Request op code */
    uint8_t rsp_code;                                       /*!< Response code */
    uint8_t rsp_parameter[6];                              /*!< Response parameter */
    uint32_t num_objects;                                   /*!< Number of objects (for Request Number of Objects) */
} esp_ble_otp_olcp_rsp_t;

/**
 * @brief OTP Transfer Channel Information
 */
typedef struct {
    esp_ble_conn_l2cap_coc_chan_t channel;                 /*!< L2CAP CoC channel handle */
    uint16_t conn_handle;                                   /*!< BLE connection handle */
    uint16_t psm;                                           /*!< PSM */
    uint16_t mtu;                                           /*!< MTU */
    esp_ble_ots_id_t object_id;                            /*!< Object ID being transferred */
    bool is_read;                                           /*!< True if reading from server, false if writing to server */
} esp_ble_otp_transfer_info_t;

/**
 * @brief OTP Event Data Union
 */
typedef union {
    struct {
        uint16_t conn_handle;                               /*!< BLE connection handle */
        uint8_t op_code;                                    /*!< OACP op code */
    } oacp_started;                                         /*!< OACP started event data */

    struct {
        uint16_t conn_handle;                               /*!< BLE connection handle */
        uint8_t op_code;                                    /*!< OACP op code */
    } oacp_aborted;                                         /*!< OACP aborted event data */

    struct {
        esp_ble_ots_feature_t feature;                     /*!< OTS Feature */
    } ots_discovered;                                       /*!< OTS discovered event data */

    struct {
        esp_ble_otp_object_info_t object_info;             /*!< Selected object information */
    } object_selected;                                      /*!< Object selected event data */

    struct {
        esp_ble_ots_change_t change;                       /*!< Object change notification */
    } object_changed;                                       /*!< Object changed event data */

    struct {
        esp_ble_otp_oacp_rsp_t response;                   /*!< OACP response */
    } oacp_response;                                        /*!< OACP response event data */

    struct {
        esp_ble_otp_olcp_rsp_t response;                   /*!< OLCP response */
    } olcp_response;                                        /*!< OLCP response event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
    } transfer_channel_connected;                           /*!< Transfer channel connected event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
    } transfer_channel_disconnected;                       /*!< Transfer channel disconnected event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
        uint8_t data[BLE_OTP_EVENT_DATA_MAX_LEN];          /*!< Copy of received data */
        uint16_t data_len;                                 /*!< Data length in this chunk */
        uint16_t chunk_offset;                              /*!< Byte offset of this chunk within the SDU (0 for single-chunk) */
        uint16_t total_len;                                 /*!< Total SDU length (same for all chunks of one SDU) */
    } transfer_data_received;                              /*!< Transfer data received event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
        uint16_t data_len;                                 /*!< Data length sent */
    } transfer_data_sent;                                   /*!< Transfer data sent event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
    } transfer_eof;                                         /*!< Transfer EOF event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
        bool success;                                       /*!< Transfer success status */
    } transfer_complete;                                    /*!< Transfer complete event data */

    struct {
        esp_ble_otp_transfer_info_t transfer_info;         /*!< Transfer channel information */
        esp_err_t error;                                    /*!< Error code */
    } transfer_error;                                       /*!< Transfer error event data */
} esp_ble_otp_event_data_t;

/**
 * @brief OTP API rules (summary)
 *
 * - OTS must be discovered before any OACP/OLCP.
 * - Feature bits gate OACP/OLCP operations.
 * - Metadata is set valid by esp_ble_otp_client_read_object_info(); call it after
 *   OLCP selection (e.g. select_first) before Read/Write/Delete when needed.
 * - Read/Write requires: OTS discovered + object selected + metadata valid.
 * - Execute requires selected object + metadata valid + execute_prop enabled.
 *   It is also used to commit write after EOF.
 * - Abort is reported only after OACP Abort response.
 *
 * Error code rules:
 * - ESP_ERR_INVALID_STATE: state prerequisites not satisfied.
 * - ESP_ERR_NOT_SUPPORTED: feature bit disabled or operation unsupported.
 * - ESP_ERR_INVALID_ARG: invalid parameters.
 * - ESP_FAIL: unexpected stack or internal error.
 */

/**
 * @brief Write Object Mode
 */
typedef enum {
    BLE_OTP_WRITE_MODE_OVERWRITE = 0x00,              /*!< Overwrite within current size */
    BLE_OTP_WRITE_MODE_TRUNCATE  = 0x01,              /*!< Truncate object after write */
    BLE_OTP_WRITE_MODE_APPEND    = 0x02,              /*!< Append beyond allocated size */
    BLE_OTP_WRITE_MODE_PATCH     = 0x03,              /*!< Patch within current size */
} esp_ble_otp_write_mode_t;

/**
 * @brief Resume read (checksum step)
 *
 * @note Feature bit `calculate_op` must be set. Caller should verify checksum
 *       and then call esp_ble_otp_client_read_object() with remaining offset.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] offset       Offset to start calculating from
 * @param[in] length       Length to calculate
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if checksum not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_resume_read_checksum(uint16_t conn_handle, uint32_t offset, uint32_t length);

/**
 * @brief Resume write using Current Size method
 *
 * @note Reads current size and resumes writing remaining bytes. Requires
 *       write_op support. For patching, use integrity method instead.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] total_size   Total object size to write
 * @param[in] mode         Write mode
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_resume_write_current_size(uint16_t conn_handle, uint32_t total_size,
                                                       esp_ble_otp_write_mode_t mode);

/**
 * @brief Resume write (checksum step)
 *
 * @note Feature bit `calculate_op` must be set. Caller should verify checksum
 *       and then call esp_ble_otp_client_write_object() for failed regions.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] offset       Offset to start calculating from
 * @param[in] length       Length to calculate
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if checksum not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_resume_write_checksum(uint16_t conn_handle, uint32_t offset, uint32_t length);

/* ========== Client APIs ========== */

/**
 * @brief Discover OTS service on the connected peer device
 *
 * @note Allowed when OTS is idle/failed. Discovery must complete before OACP/OLCP.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_discover_ots(uint16_t conn_handle);

/**
 * @brief Read OTS Feature characteristic
 *
 * @note Required before OACP/OLCP. Feature bits gate supported operations.
 *
 * @param[in]  conn_handle  BLE connection handle
 * @param[out] feature      Pointer to store OTS Feature
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_read_feature(uint16_t conn_handle, esp_ble_ots_feature_t *feature);

/**
 * @brief Select first object in the list
 *
 * @note Allowed only when session is idle. Selection must be stable before OACP.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_first(uint16_t conn_handle);

/**
 * @brief Select last object in the list
 *
 * @note Allowed only when session is idle. Selection must be stable before OACP.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_last(uint16_t conn_handle);

/**
 * @brief Select previous object in the list
 *
 * @note Allowed only when session is idle. Selection must be stable before OACP.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_previous(uint16_t conn_handle);

/**
 * @brief Select next object in the list
 *
 * @note Allowed only when session is idle. Selection must be stable before OACP.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_next(uint16_t conn_handle);

/**
 * @brief Select object by ID
 *
 * @note Allowed only when session is idle. Selection must be stable before OACP.
 *       Requires OLCP Go To support. Filters are ignored by Go To and reset to No Filter.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] object_id    Object ID to select
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_by_id(uint16_t conn_handle, const esp_ble_ots_id_t *object_id);

/**
 * @brief Select object by index
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] index        Object index (0-based)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_by_index(uint16_t conn_handle, uint32_t index);

/**
 * @brief Set object list sort order
 *
 * @note Allowed only when session is idle.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] sort_order   Sort order (BLE_OTS_OLCP_SORT_BY_*)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_set_sort_order(uint16_t conn_handle, uint8_t sort_order);

/**
 * @brief Request number of objects
 *
 * @note Allowed only when session is idle.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_request_num_objects(uint16_t conn_handle);

/**
 * @brief Clear marking of objects
 *
 * @note Allowed only when session is idle.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_clear_mark(uint16_t conn_handle);

/**
 * @brief Set object list filter
 *
 * @note Allowed only when session is idle.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] filter       Filter type and parameter
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_set_filter(uint16_t conn_handle, const esp_ble_ots_filter_t *filter);

/**
 * @brief Read current object information
 *
 * @note Requires object selected. Updates metadata state to VALID.
 *       Call this after OLCP selection (e.g. select_first) before read_object,
 *       write_object, or delete_object so that metadata is valid.
 *
 * @param[in]  conn_handle   BLE connection handle
 * @param[out] object_info   Pointer to store object information
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_read_object_info(uint16_t conn_handle, esp_ble_otp_object_info_t *object_info);

/**
 * @brief Write Object Name metadata
 *
 * @note Requires object selected. Name must be non-empty and contain no ASCII control characters.
 *       Long write is not supported; if length exceeds ATT_MTU-3, return ESP_ERR_NOT_SUPPORTED.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] name         Object name buffer
 * @param[in] name_len     Object name length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if long write is required
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_write_name(uint16_t conn_handle, const uint8_t *name, size_t name_len);

/**
 * @brief Write Object Properties metadata
 *
 * @note Requires object selected.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] prop         Object properties
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_write_prop(uint16_t conn_handle, const esp_ble_ots_prop_t *prop);

#ifdef CONFIG_BLE_OTS_FIRST_CREATED_CHARACTERISTIC_ENABLE
/**
 * @brief Write Object First-Created metadata
 *
 * @note Requires object selected. First-Created is intended to be written once.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] utc          UTC time
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_write_first_created(uint16_t conn_handle, const esp_ble_ots_utc_t *utc);
#endif

#ifdef CONFIG_BLE_OTS_LAST_MODIFIED_CHARACTERISTIC_ENABLE
/**
 * @brief Write Object Last-Modified metadata
 *
 * @note Requires object selected.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] utc          UTC time
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_write_last_modified(uint16_t conn_handle, const esp_ble_ots_utc_t *utc);
#endif

/**
 * @brief Discover all objects (OLCP First)
 *
 * @note Requires OTS discovered and OLCP supported.
 *       Caller should iterate with esp_ble_otp_client_discover_next().
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if OLCP not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_discover_all_start(uint16_t conn_handle);

/**
 * @brief Discover next object (OLCP Next)
 *
 * @note Requires OTS discovered and OLCP supported.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if OLCP not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_discover_next(uint16_t conn_handle);

/**
 * @brief Discover objects by filter
 *
 * @note Requires OTS discovered, OLCP supported, and Object List Filter exposed.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] filter       Filter to apply
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if OLCP or filter not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_discover_by_filter(uint16_t conn_handle, const esp_ble_ots_filter_t *filter);

/**
 * @brief Select the Directory Listing Object (Object ID 0x000000000000)
 *
 * @note Requires OTS discovered and OLCP Go-To supported.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if OLCP not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_select_directory_listing(uint16_t conn_handle);

/**
 * @brief Read Directory Listing Object contents
 *
 * @note Selects Directory Listing Object (ID 0x000000000000), reads current size,
 *       and starts OACP Read for the full contents.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if OLCP not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_read_directory_listing(uint16_t conn_handle);

/**
 * @brief Create a new object
 *
 * @note Feature bit `create_op` must be set.
 *       After success, client should write a valid Object Name (non-empty,
 *       no ASCII control characters). If disconnected before naming, the
 *       server may delete the object as malformed.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] object_type  Object type
 * @param[in] object_size  Allocated object size
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_create_object(uint16_t conn_handle, uint16_t object_type, uint32_t object_size);

/**
 * @brief Delete current object
 *
 * @note Requires object selected + metadata valid + server object property
 *       delete_prop set. Feature bit `delete_op` must be set.
 *       Server may respond with OACP RSP Invalid Object (e.g. object not found/deleted).
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_ERR_NOT_SUPPORTED if object's delete property is not set
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_delete_object(uint16_t conn_handle);

/**
 * @brief Read object data via Object Transfer Channel
 *
 * @note Requires: OTS discovered + object selected + metadata valid.
 *       Feature bit `read_op` must be set.
 *       After OLCP selection (e.g. select_first), metadata is UNKNOWN; call
 *       esp_ble_otp_client_read_object_info() first, or this returns ESP_ERR_INVALID_STATE.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] offset       Offset to start reading from
 * @param[in] length       Length to read (0 = read all remaining data)
 *
 * @return
 *  - ESP_OK on success (transfer will be initiated, wait for events)
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role, or metadata not valid
 *  - ESP_ERR_NOT_SUPPORTED if read_op not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_read_object(uint16_t conn_handle, uint32_t offset, uint32_t length);

/**
 * @brief Write object data via Object Transfer Channel
 *
 * @note Requires: OTS discovered + object selected + metadata valid.
 *       Feature bit `write_op` must be set.
 *       Object Properties must permit write.
 *       After OLCP selection, call esp_ble_otp_client_read_object_info() first if metadata not valid.
 *       Offset must be <= Current Size; Offset + Length must be <= Allocated Size,
 *       unless mode requests append/truncate/patch and the corresponding feature
 *       bit is supported.
 *       For resume (Current Size method), use the current size as the offset.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] offset       Offset to start writing at
 * @param[in] length       Length to write
 * @param[in] mode         Write mode
 *
 * @return
 *  - ESP_OK on success (transfer will be initiated, wait for events)
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role, or metadata not valid
 *  - ESP_ERR_NOT_SUPPORTED if write_op not supported or object properties do not permit write
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_write_object(uint16_t conn_handle, uint32_t offset, uint32_t length,
                                          esp_ble_otp_write_mode_t mode);

/**
 * @brief Calculate checksum of object data
 *
 * @note Feature bit `calculate_op` must be set.
 *       This API is used by resume procedures to verify partial data.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] offset       Offset to start calculating from
 * @param[in] length       Length to calculate (0 = calculate all remaining data)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_calculate_checksum(uint16_t conn_handle, uint32_t offset, uint32_t length);

/**
 * @brief Execute object (if supported)
 *
 * @note Requires selected object + metadata valid + execute_prop enabled.
 *       Feature bit `execute_op` must be set.
 *       If committing a write, this is allowed after EOF even when session is busy.
 *
 * @param[in] conn_handle  BLE connection handle
 * @param[in] parameters   Execute parameters (optional, can be NULL)
 * @param[in] param_len    Parameter length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_execute_object(uint16_t conn_handle, const uint8_t *parameters, uint8_t param_len);

/**
 * @brief Abort current object operation
 *
 * @note ABORTED event is reported only after OACP Abort response.
 *
 * @param[in] conn_handle  BLE connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in client role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_abort(uint16_t conn_handle);

/**
 * @brief Send data over Object Transfer Channel (after read/write operation initiated)
 *
 * @param[in] transfer_info  Transfer channel information (from event)
 * @param[in] data            Data to send
 * @param[in] data_len        Data length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if transfer channel is not ready
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_send_data(const esp_ble_otp_transfer_info_t *transfer_info,
                                       const uint8_t *data, uint16_t data_len);

/**
 * @brief Disconnect Object Transfer Channel
 *
 * @param[in] transfer_info  Transfer channel information (from event)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_client_disconnect_transfer_channel(const esp_ble_otp_transfer_info_t *transfer_info);

/* ========== Server APIs ========== */

/**
 * @brief Set OTS feature (Server only)
 *
 * @param[in] feature  OTS Feature to set
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in server role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_server_set_feature(const esp_ble_ots_feature_t *feature);

/**
 * @brief Register callback for OACP operations (Server only)
 *
 * @param[in] callback  Callback function to handle OACP operations
 *                      Return ESP_OK to accept, ESP_ERR_* to reject with corresponding error code
 * @param[in] ctx       User context passed to the callback
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in server role
 */
esp_err_t esp_ble_otp_server_register_oacp_callback(esp_err_t (*callback)(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx), void *ctx);

/**
 * @brief Register callback for OLCP operations (Server only)
 *
 * @param[in] callback  Callback function to handle OLCP operations
 *                      Return ESP_OK to accept, ESP_ERR_* to reject with corresponding error code
 * @param[in] ctx       User context passed to the callback
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in server role
 */
esp_err_t esp_ble_otp_server_register_olcp_callback(esp_err_t (*callback)(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx), void *ctx);

/**
 * @brief Send OACP response (Server only)
 *
 * @param[in] req_op_code    Request op code
 * @param[in] rsp_code       Response code
 * @param[in] rsp_parameter  Response parameter (can be NULL)
 * @param[in] param_len      Parameter length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in server role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_server_send_oacp_response(uint8_t req_op_code, uint8_t rsp_code,
                                                const uint8_t *rsp_parameter, uint8_t param_len);

/**
 * @brief Send OLCP response (Server only)
 *
 * @param[in] req_op_code    Request op code
 * @param[in] rsp_code       Response code
 * @param[in] rsp_parameter  Response parameter (can be NULL)
 * @param[in] param_len      Parameter length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized or not in server role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_server_send_olcp_response(uint8_t req_op_code, uint8_t rsp_code,
                                                const uint8_t *rsp_parameter, uint8_t param_len);

/**
 * @brief Send data over Object Transfer Channel (Server only)
 *
 * @param[in] transfer_info  Transfer channel information (from event)
 * @param[in] data            Data to send
 * @param[in] data_len        Data length
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if transfer channel is not ready
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_server_send_data(const esp_ble_otp_transfer_info_t *transfer_info,
                                       const uint8_t *data, uint16_t data_len);

/**
 * @brief Disconnect Object Transfer Channel (Server only)
 *
 * @param[in] transfer_info  Transfer channel information (from event)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_server_disconnect_transfer_channel(const esp_ble_otp_transfer_info_t *transfer_info);

/* ========== Common APIs ========== */

/**
 * @brief Initialize OTP
 *
 * @param[in] config  OTP configuration
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NO_MEM if memory allocation failed
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_init(const esp_ble_otp_config_t *config);

/**
 * @brief Deinitialize OTP
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if OTP is not initialized
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_otp_deinit(void);

#ifdef __cplusplus
}
#endif
