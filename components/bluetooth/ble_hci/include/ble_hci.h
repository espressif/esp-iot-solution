/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "esp_err.h"

/// Advertising data maximum length
#define ESP_BLE_ADV_DATA_LEN_MAX               31
/// Scan response data maximum length
#define ESP_BLE_SCAN_RSP_DATA_LEN_MAX          31
/// BLE Address Length
#define BLE_HCI_ADDR_LEN                       6

/// Bluetooth device address
typedef uint8_t ble_hci_addr_t[BLE_HCI_ADDR_LEN];

/**
 * @brief Sub Event of BLE_HCI_BLE_SCAN_RESULT_EVT
 *
 */
typedef enum {
    BLE_HCI_SEARCH_INQ_RES_EVT             = 0,      /*!< Inquiry result for a peer device. */
    BLE_HCI_SEARCH_INQ_CMPL_EVT            = 1,      /*!< Inquiry complete. */
    BLE_HCI_SEARCH_DISC_RES_EVT            = 2,      /*!< Discovery result for a peer device. */
    BLE_HCI_SEARCH_DISC_BLE_RES_EVT        = 3,      /*!< Discovery result for BLE GATT based service on a peer device. */
    BLE_HCI_SEARCH_DISC_CMPL_EVT           = 4,      /*!< Discovery complete. */
    BLE_HCI_SEARCH_DI_DISC_CMPL_EVT        = 5,      /*!< Discovery complete. */
    BLE_HCI_SEARCH_SEARCH_CANCEL_CMPL_EVT  = 6,      /*!< Search cancelled */
    BLE_HCI_SEARCH_INQ_DISCARD_NUM_EVT     = 7,      /*!< The number of pkt discarded by flow control */
} ble_hci_search_evt_t;

/**
 * @brief BLE address type
 *
 */
typedef enum {
    BLE_ADDR_TYPE_PUBLIC        = 0x00,     /*!< Public Device Address */
    BLE_ADDR_TYPE_RANDOM        = 0x01,     /*!< Random Device Address.  */
    BLE_ADDR_TYPE_RPA_PUBLIC    = 0x02,     /*!< Resolvable Private Address (RPA) with public identity address */
    BLE_ADDR_TYPE_RPA_RANDOM    = 0x03,     /*!< Resolvable Private Address (RPA) with random identity address. */
} ble_hci_addr_type_t;

/**
 * @brief Bluetooth device type
 *
 */
typedef enum {
    BLE_HCI_DEVICE_TYPE_BREDR   = 0x01,
    BLE_HCI_DEVICE_TYPE_BLE     = 0x02,
    BLE_HCI_DEVICE_TYPE_DUMO    = 0x03,
} ble_hci_dev_type_t;

/**
 * @brief BLE advertising type
 *
 */
typedef enum {
    ADV_TYPE_IND                = 0x00,
    ADV_TYPE_DIRECT_IND_HIGH    = 0x01,
    ADV_TYPE_SCAN_IND           = 0x02,
    ADV_TYPE_NONCONN_IND        = 0x03,
    ADV_TYPE_DIRECT_IND_LOW     = 0x04,
} ble_hci_adv_type_t;

/**
 * @brief BLE scan result struct
 *
 */
typedef struct  {
    ble_hci_search_evt_t search_evt;            /*!< Search event type */
    ble_hci_dev_type_t dev_type;                 /*!< Device type */
    ble_hci_addr_t bda;                          /*!< Bluetooth device address which has been searched */
    ble_hci_addr_type_t ble_addr_type;          /*!< Ble device address type */
    uint8_t  ble_adv[ESP_BLE_ADV_DATA_LEN_MAX + ESP_BLE_SCAN_RSP_DATA_LEN_MAX];     /*!< Received EIR */
    uint8_t adv_data_len;                       /*!< Adv data length */
    uint8_t scan_rsp_len;                       /*!< Scan response length */
    int rssi;                                   /*!< Searched device's RSSI */
} ble_hci_scan_result_t;

/**
 * @brief Advertising channel mask
 *
 */
typedef enum {
    ADV_CHNL_37     = 0x01,
    ADV_CHNL_38     = 0x02,
    ADV_CHNL_39     = 0x04,
    ADV_CHNL_ALL    = 0x07,
} ble_hci_adv_channel_t;

/**
 * @brief Ble scan type
 *
 */
typedef enum {
    BLE_SCAN_TYPE_PASSIVE   =   0x0,            /*!< Passive scan */
    BLE_SCAN_TYPE_ACTIVE    =   0x1,            /*!< Active scan */
} ble_hci_scan_type_t;

/**
 * @brief Ble adv filteer type
 *
 */
typedef enum {
    ///Allow both scan and connection requests from anyone
    ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY  = 0x00,
    ///Allow both scan req from White List devices only and connection req from anyone
    ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY,
    ///Allow both scan req from anyone and connection req from White List devices only
    ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST,
    ///Allow scan and connection requests from White List devices only
    ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST,
    ///Enumeration end value for advertising filter policy value check
} ble_hci_adv_filter_t;

/**
 * @brief Ble adv parameters
 *
 */
typedef struct {
    uint16_t adv_int_min;   /*!< Time = N × 0.625 ms Range: 0x0020 to 0x4000 */
    uint16_t adv_int_max;   /*!< Time = N × 0.625 ms Range: 0x0020 to 0x4000 */
    ble_hci_adv_type_t adv_type;       /*!< Advertising Type */
    ble_hci_addr_type_t own_addr_type;  /*!< Own Address Type */
    ble_hci_addr_t           peer_addr;          /*!< Peer device bluetooth device address */
    ble_hci_addr_type_t     peer_addr_type;     /*!< Peer device bluetooth device address type, only support public address type and random address type */
    ble_hci_adv_channel_t   channel_map;    /*!< Advertising channel map */
    ble_hci_adv_filter_t  adv_filter_policy; /*!< Advertising Filter Policy: */
} ble_hci_adv_param_t;

/**
 * @brief Ble sccan parameters
 *
 */
typedef struct {
    ble_hci_scan_type_t scan_type;      /*!< Scan Type */
    uint16_t scan_interval; /*!< Time = N × 0.625 ms Range: 0x0004 to 0x4000 */
    uint16_t scan_window;   /*!< Time = N × 0.625 ms Range: 0x0004 to 0x4000 */
    ble_hci_addr_type_t  own_addr_type;  /*!< Own Address Type */
    ble_hci_adv_filter_t filter_policy;  /*!< Scanning Filter Policy */
} ble_hci_scan_param_t;

/**
 * @brief BLE HCI scan callback function type
 * @param scan_result : ble advertisement scan result
 * @param result_len :  length of scan result
 */
typedef void (*ble_hci_scan_cb_t)(ble_hci_scan_result_t *scan_result, uint16_t result_len);

/**
 * @brief BLE HCI initialization
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_init(void);

/**
 * @brief BLE HCI de-initialization
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_deinit(void);

/**
 * @brief BLE HCI reset controller
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_reset(void);

/**
 * @brief Enable BLE HCI meta event
 *
 * @return esp_err_t
 */
esp_err_t ble_hci_enable_meta_event(void);

/**
 * @brief Set BLE HCI advertising parameters
 *
 * @param param : advertising parameters
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_adv_param(ble_hci_adv_param_t *param);

/**
 * @brief Set BLE HCI advertising data
 *
 * @param len : advertising data length
 * @param data : advertising data
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_adv_data(uint8_t len, uint8_t *data);

/**
 * @brief Set BLE HCI advertising enable
 *
 * @param enable : true for enable advertising
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_adv_enable(bool enable);

/**
 * @brief Set BLE HCI scan parameters
 *
 * @param param : scan parameters
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_scan_param(ble_hci_scan_param_t* param);

/**
 * @brief Set BLE HCI scan enable
 *
 * @param enable : enable or disable scan
 * @param filter_duplicates : filter duplicates or not
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_scan_enable(bool enable, bool filter_duplicates);

/**
 * @brief Set BLE HCI scan callback
 *
 * @param cb : scan callback function pointer
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_register_scan_callback(ble_hci_scan_cb_t cb);

/**
 * @brief Add BLE white list
 *
 * @param addr : address to be added to white list
 * @param addr_type : address type to be added to white list
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_add_to_accept_list(ble_hci_addr_t addr, ble_hci_addr_type_t addr_type);

/**
 * @brief Clear BLE white list
 *
 * @return esp_err_t
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_clear_accept_list(void);

/**
 * @brief Set BLE owner address
 *
 * @param addr : owner address
 * @return
 *    - ESP_OK: succeed
 *    - others: fail
 */
esp_err_t ble_hci_set_random_address(ble_hci_addr_t addr);

#ifdef __cplusplus
}
#endif
