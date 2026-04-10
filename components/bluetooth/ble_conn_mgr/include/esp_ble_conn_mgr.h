/*
 * SPDX-FileCopyrightText: 2019-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_event.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   BLE address format string for printf-style logging
 */
#define BLE_CONN_MGR_ADDR_STR "%02x:%02x:%02x:%02x:%02x:%02x"
/**
 * @brief   BLE address formatter arguments for printf-style logging
 */
#define BLE_CONN_MGR_ADDR_HEX(addr) ((addr)[5]), ((addr)[4]), ((addr)[3]), ((addr)[2]), ((addr)[1]), ((addr)[0])

/** Common advertising data types */
#define ESP_BLE_CONN_ADV_TYPE_FLAGS            0x01
#define ESP_BLE_CONN_ADV_TYPE_NAME_SHORT       0x08
#define ESP_BLE_CONN_ADV_TYPE_NAME_COMPLETE    0x09
#define ESP_BLE_CONN_ADV_TYPE_TX_POWER         0x0A
#define ESP_BLE_CONN_ADV_TYPE_UUID16_INCOMP    0x02
#define ESP_BLE_CONN_ADV_TYPE_UUID16_COMPLETE  0x03
#define ESP_BLE_CONN_ADV_TYPE_UUID32_INCOMP    0x04
#define ESP_BLE_CONN_ADV_TYPE_UUID32_COMPLETE  0x05
#define ESP_BLE_CONN_ADV_TYPE_UUID128_INCOMP   0x06
#define ESP_BLE_CONN_ADV_TYPE_UUID128_COMPLETE 0x07
#define ESP_BLE_CONN_ADV_TYPE_SERVICE_DATA16   0x16
#define ESP_BLE_CONN_ADV_TYPE_SERVICE_DATA32   0x20
#define ESP_BLE_CONN_ADV_TYPE_SERVICE_DATA128  0x21
#define ESP_BLE_CONN_ADV_TYPE_APPEARANCE       0x19
#define ESP_BLE_CONN_ADV_TYPE_MANUFACTURER     0xFF

#define ESP_BLE_CONN_ADV_DATA_MAX_LEN          31

/** @brief Invalid connection handle (no connection). Use to check "not connected" or invalid handle. */
#define BLE_CONN_HANDLE_INVALID                0xFFFF
/** @brief Maximum valid connection handle. Valid range is 0x0000 – BLE_CONN_HANDLE_MAX (0x0EFF). */
#define BLE_CONN_HANDLE_MAX                    0x0EFF

/**
 * @brief   Passkey action types for Security Manager pairing
 */
typedef enum {
    ESP_BLE_CONN_SM_ACT_NONE   = 0,  /*!< No action / unknown / other */
    ESP_BLE_CONN_SM_ACT_OOB    = 1,  /*!< Out-of-band (OOB) data required */
    ESP_BLE_CONN_SM_ACT_INPUT  = 2,  /*!< User must enter passkey; call esp_ble_conn_passkey_reply() */
    ESP_BLE_CONN_SM_ACT_DISP   = 3,  /*!< Display passkey to user, application should display the passkey */
    ESP_BLE_CONN_SM_ACT_NUMCMP = 4,  /*!< Numeric comparison; call esp_ble_conn_numcmp_reply() */
} esp_ble_conn_sm_action_t;

/**
 * @brief   L2CAP CoC channel handle (opaque pointer)
 *
 * @note This is an opaque handle managed by the stack. Applications must not
 *       dereference, cast, or persist it beyond the lifetime of the associated
 *       connection. Use it only with the L2CAP CoC APIs provided here.
 */
typedef void *esp_ble_conn_l2cap_coc_chan_t;

/**
 * @brief   L2CAP CoC channel information
 */
typedef struct {
    uint16_t scid;            /*!< Source Channel ID */
    uint16_t dcid;            /*!< Destination Channel ID */
    uint16_t our_l2cap_mtu;   /*!< Local L2CAP MTU */
    uint16_t peer_l2cap_mtu;  /*!< Peer L2CAP MTU */
    uint16_t psm;             /*!< Protocol/Service Multiplexer */
    uint16_t our_coc_mtu;     /*!< Local CoC MTU */
    uint16_t peer_coc_mtu;    /*!< Peer CoC MTU */
} esp_ble_conn_l2cap_coc_chan_info_t;

/**
 * @brief   L2CAP CoC SDU buffer
 */
typedef struct {
    uint16_t len;  /*!< SDU length in bytes */
    uint8_t *data; /*!< SDU data buffer */
} esp_ble_conn_l2cap_coc_sdu_t;

/**
 * @brief   BLE scan result data
 */
typedef struct {
    uint8_t addr[6];                          /*!< Peer address */
    uint8_t addr_type;                        /*!< Peer address type */
    int8_t rssi;                              /*!< RSSI in dBm */
    uint8_t adv_data_len;                     /*!< Advertising data length */
    uint8_t adv_data[ESP_BLE_CONN_ADV_DATA_MAX_LEN]; /*!< Advertising data */
} esp_ble_conn_scan_result_t;

typedef bool (*esp_ble_conn_scan_cb_t)(const esp_ble_conn_scan_result_t *result, void *arg);

/**
 * @brief   L2CAP CoC event types
 */
typedef enum {
    ESP_BLE_CONN_L2CAP_COC_EVENT_CONNECTED = 0,   /*!< CoC channel connected */
    ESP_BLE_CONN_L2CAP_COC_EVENT_DISCONNECTED,    /*!< CoC channel disconnected */
    ESP_BLE_CONN_L2CAP_COC_EVENT_ACCEPT,          /*!< CoC connection accepted */
    ESP_BLE_CONN_L2CAP_COC_EVENT_DATA_RECEIVED,   /*!< SDU received */
    ESP_BLE_CONN_L2CAP_COC_EVENT_TX_UNSTALLED,    /*!< TX unstalled */
    ESP_BLE_CONN_L2CAP_COC_EVENT_RECONFIG_COMPLETED, /*!< Local reconfigure completed */
    ESP_BLE_CONN_L2CAP_COC_EVENT_PEER_RECONFIGURED,  /*!< Peer reconfigured */
} esp_ble_conn_l2cap_coc_event_type_t;

/**
 * @brief   L2CAP CoC event payload
 */
typedef struct esp_ble_conn_l2cap_coc_event {
    esp_ble_conn_l2cap_coc_event_type_t type; /*!< Event type */
    union {
        struct {
            int status;                       /*!< Connection status (0 on success) */
            uint16_t conn_handle;             /*!< Connection handle */
            esp_ble_conn_l2cap_coc_chan_t chan; /*!< CoC channel */
        } connect; /*!< Connected event data */
        struct {
            uint16_t conn_handle;             /*!< Connection handle */
            esp_ble_conn_l2cap_coc_chan_t chan; /*!< CoC channel */
        } disconnect; /*!< Disconnected event data */
        struct {
            uint16_t conn_handle;             /*!< Connection handle */
            uint16_t peer_sdu_size;           /*!< Peer SDU size */
            esp_ble_conn_l2cap_coc_chan_t chan; /*!< CoC channel */
        } accept; /*!< Accept event data */
        struct {
            uint16_t conn_handle;             /*!< Connection handle */
            esp_ble_conn_l2cap_coc_chan_t chan; /*!< CoC channel */
            esp_ble_conn_l2cap_coc_sdu_t sdu;  /*!< SDU payload. Note: The sdu.data buffer is managed internally
                                                    and will be freed immediately after the callback returns.
                                                    The callback must not save the pointer for later use. If
                                                    asynchronous processing is needed, the callback should copy
                                                    the data. */
        } receive; /*!< Receive event data */
        struct {
            uint16_t conn_handle;             /*!< Connection handle */
            esp_ble_conn_l2cap_coc_chan_t chan; /*!< CoC channel */
            int status;                       /*!< TX status */
        } tx_unstalled; /*!< TX unstalled event data */
        struct {
            int status;                       /*!< Reconfigure status */
            uint16_t conn_handle;             /*!< Connection handle */
            esp_ble_conn_l2cap_coc_chan_t chan; /*!< CoC channel */
        } reconfigured; /*!< Reconfigure event data */
    };
} esp_ble_conn_l2cap_coc_event_t;

/**
 * @brief   L2CAP CoC event callback
 *
 * @param[in] event  L2CAP CoC event payload
 * @param[in] arg    User context
 *
 * @note For ESP_BLE_CONN_L2CAP_COC_EVENT_DATA_RECEIVED events, the sdu.data buffer
 *       is managed internally and will be freed immediately after this callback returns.
 *       The callback must not save the pointer for later use. If asynchronous processing
 *       is needed, the callback should copy the data before returning.
 *
 * @return
 *  - 0 to indicate the event was handled successfully
 *  - Non-zero to indicate an application error (stack behavior may vary by event type)
 */
typedef int (*esp_ble_conn_l2cap_coc_event_cb_t)(esp_ble_conn_l2cap_coc_event_t *event, void *arg);

/**
 * @brief   Initialize L2CAP CoC SDU memory pool
 *
 * @note This API is thread-safe, but should be called from task context.
 *       Repeated calls are safe and return ESP_OK.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_TIMEOUT if lock acquisition times out
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_mem_init(void);

/**
 * @brief   Release L2CAP CoC SDU memory pool
 *
 * @note This API is thread-safe, but should be called from task context.
 *       It must only be called when there are no active L2CAP CoC servers or channels.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_TIMEOUT if lock acquisition times out
 *  - ESP_ERR_INVALID_STATE if active L2CAP CoC contexts exist
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_mem_release(void);

/**
 * @brief   Provide receive buffer for L2CAP CoC channel (accept or next SDU)
 *
 * @param[in] conn_handle    Connection handle
 * @param[in] peer_sdu_size  Peer SDU size from accept event
 * @param[in] chan           L2CAP CoC channel
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NO_MEM if memory pool not initialized or allocation failed
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_accept(uint16_t conn_handle, uint16_t peer_sdu_size,
                                        esp_ble_conn_l2cap_coc_chan_t chan);

/**
 * Success code and error codes
 */
#define ESP_IOT_ATT_SUCCESS                        0x00
#define ESP_IOT_ATT_INVALID_HANDLE                 0x01
#define ESP_IOT_ATT_READ_NOT_PERMIT                0x02
#define ESP_IOT_ATT_WRITE_NOT_PERMIT               0x03
#define ESP_IOT_ATT_INVALID_PDU                    0x04
#define ESP_IOT_ATT_INSUF_AUTHENTICATION           0x05
#define ESP_IOT_ATT_REQ_NOT_SUPPORTED              0x06
#define ESP_IOT_ATT_INVALID_OFFSET                 0x07
#define ESP_IOT_ATT_INSUF_AUTHORIZATION            0x08
#define ESP_IOT_ATT_PREPARE_Q_FULL                 0x09
#define ESP_IOT_ATT_NOT_FOUND                      0x0a
#define ESP_IOT_ATT_NOT_LONG                       0x0b
#define ESP_IOT_ATT_INSUF_KEY_SIZE                 0x0c
#define ESP_IOT_ATT_INVALID_ATTR_LEN               0x0d
#define ESP_IOT_ATT_ERR_UNLIKELY                   0x0e
#define ESP_IOT_ATT_INSUF_ENCRYPTION               0x0f
#define ESP_IOT_ATT_UNSUPPORT_GRP_TYPE             0x10
#define ESP_IOT_ATT_INSUF_RESOURCE                 0x11
#define ESP_IOT_ATT_DATABASE_OUT_OF_SYNC           0x12
#define ESP_IOT_ATT_VALUE_NOT_ALLOWED              0x13

/**
 * Application error codes
 */
#define ESP_IOT_ATT_NO_RESOURCES                   0x80
#define ESP_IOT_ATT_INTERNAL_ERROR                 0x81
#define ESP_IOT_ATT_WRONG_STATE                    0x82
#define ESP_IOT_ATT_DB_FULL                        0x83
#define ESP_IOT_ATT_BUSY                           0x84
#define ESP_IOT_ATT_ERROR                          0x85
#define ESP_IOT_ATT_CMD_STARTED                    0x86
#define ESP_IOT_ATT_ILLEGAL_PARAMETER              0x87
#define ESP_IOT_ATT_PENDING                        0x88
#define ESP_IOT_ATT_AUTH_FAIL                      0x89
#define ESP_IOT_ATT_MORE                           0x8a
#define ESP_IOT_ATT_INVALID_CFG                    0x8b
#define ESP_IOT_ATT_SERVICE_STARTED                0x8c
#define ESP_IOT_ATT_ENCRYPED_MITM                  ESP_IOT_ATT_SUCCESS
#define ESP_IOT_ATT_ENCRYPED_NO_MITM               0x8d
#define ESP_IOT_ATT_NOT_ENCRYPTED                  0x8e
#define ESP_IOT_ATT_CONGESTED                      0x8f
#define ESP_IOT_ATT_DUP_REG                        0x90
#define ESP_IOT_ATT_ALREADY_OPEN                   0x91
#define ESP_IOT_ATT_CANCEL                         0x92

/**
 * Common profile and service error codes
 */
#define ESP_IOT_ATT_STACK_RSP                      0xE0
#define ESP_IOT_ATT_APP_RSP                        0xE1
#define ESP_IOT_ATT_UNKNOWN_ERROR                  0XEF
#define ESP_IOT_ATT_CCC_CFG_ERR                    0xFD
#define ESP_IOT_ATT_PRC_IN_PROGRESS                0xFE
#define ESP_IOT_ATT_OUT_OF_RANGE                   0xFF

/**
 * BLE device name cannot be larger than this value
 * 31 bytes (max scan response size) - 1 byte (length) - 1 byte (type) = 29 bytes
 */
#define MAX_BLE_DEVNAME_LEN                 29      /*!< BLE device name length */
#define BLE_UUID128_VAL_LEN                 16      /*!< 128 bit UUID length */
#define BROADCAST_PARAM_LEN                 15      /*!< BLE device broadcast param length */

#define BLE_CONN_GATT_CHR_BROADCAST         0x0001  /*!< Characteristic broadcast properties */
#define BLE_CONN_GATT_CHR_READ              0x0002  /*!< Characteristic read properties */
#define BLE_CONN_GATT_CHR_WRITE_NO_RSP      0x0004  /*!< Characteristic write properties */
#define BLE_CONN_GATT_CHR_WRITE             0x0008  /*!< Characteristic write properties */
#define BLE_CONN_GATT_CHR_NOTIFY            0x0010  /*!< Characteristic notify properties */
#define BLE_CONN_GATT_CHR_INDICATE          0x0020  /*!< Characteristic indicate properties */
#define BLE_CONN_GATT_CHR_READ_ENC          0x0200  /*!< Characteristic read requires encryption */
#define BLE_CONN_GATT_CHR_READ_AUTHEN       0x0400  /*!< Characteristic read requires authentication */
#define BLE_CONN_GATT_CHR_READ_AUTHOR       0x0800  /*!< Characteristic read requires authorization */
#define BLE_CONN_GATT_CHR_WRITE_ENC         0x1000  /*!< Characteristic write requires encryption */
#define BLE_CONN_GATT_CHR_WRITE_AUTHEN      0x2000  /*!< Characteristic write requires authentication */
#define BLE_CONN_GATT_CHR_WRITE_AUTHOR      0x4000  /*!< Characteristic write requires authorization */

/** Legacy advertising connection mode (conn_mode in esp_ble_conn_adv_params_t). Values match the underlying BLE stack. */
typedef enum {
    ESP_BLE_CONN_ADV_CONN_MODE_NON = 0,   /*!< Non-connectable */
    ESP_BLE_CONN_ADV_CONN_MODE_DIR = 1,   /*!< Connectable directed */
    ESP_BLE_CONN_ADV_CONN_MODE_UND = 2,   /*!< Connectable undirected */
} esp_ble_conn_adv_conn_mode_t;

/** Legacy advertising discoverable mode (disc_mode in esp_ble_conn_adv_params_t) */
typedef enum {
    ESP_BLE_CONN_ADV_DISC_MODE_NON = 0,   /*!< Non-discoverable */
    ESP_BLE_CONN_ADV_DISC_MODE_LTD = 1,   /*!< Limited discoverable */
    ESP_BLE_CONN_ADV_DISC_MODE_GEN = 2,   /*!< General discoverable */
} esp_ble_conn_adv_disc_mode_t;

/** BLE address type (for own_addr_type, peer_addr_type in adv/scan/event data) */
typedef enum {
    ESP_BLE_CONN_ADDR_PUBLIC = 0,         /*!< Public device address */
    ESP_BLE_CONN_ADDR_RANDOM = 1,        /*!< Random device address */
} esp_ble_conn_addr_type_t;

/** LE PHY (primary_phy, secondary_phy in adv params; scan_phys bitmask) */
typedef enum {
    ESP_BLE_CONN_PHY_1M    = 1,           /*!< 1M PHY */
    ESP_BLE_CONN_PHY_2M    = 2,           /*!< 2M PHY */
    ESP_BLE_CONN_PHY_CODED = 3,           /*!< Coded PHY */
} esp_ble_conn_phy_t;

/** LE PHY preference mask (for set preferred PHY). Values match the underlying stack. */
#define ESP_BLE_CONN_PHY_MASK_1M    0x01   /*!< Prefer 1M PHY */
#define ESP_BLE_CONN_PHY_MASK_2M    0x02   /*!< Prefer 2M PHY */
#define ESP_BLE_CONN_PHY_MASK_CODED 0x04   /*!< Prefer Coded PHY */
#define ESP_BLE_CONN_PHY_MASK_ALL   0x07   /*!< All PHYs (1M | 2M | Coded) */

/** Coded PHY option (phy_opts when using ESP_BLE_CONN_PHY_MASK_CODED). */
#define ESP_BLE_CONN_PHY_CODED_OPT_ANY  0   /*!< Coded: any */
#define ESP_BLE_CONN_PHY_CODED_OPT_S2   1   /*!< Coded: S=2 */
#define ESP_BLE_CONN_PHY_CODED_OPT_S8   2   /*!< Coded: S=8 */

/** Scan filter policy (filter_policy in scan params) */
typedef enum {
    ESP_BLE_CONN_SCAN_FILT_NO_WL  = 0,    /*!< Accept any advertiser */
    ESP_BLE_CONN_SCAN_FILT_USE_WL = 1,    /*!< Use whitelist only */
} esp_ble_conn_scan_filt_policy_t;

/**
 * @brief This is type of function that will handle the registered characteristic
 *
 * @param[in] inbuf         The pointer to store data: read operation if NULL or write operation if not NULL
 * @param[in] inlen         The store data length
 * @param[out] outbuf       Variable to store data, it'll free by connection management component
 * @param[out] outlen       Variable to store data length
 * @param[in] priv_data     Private data context
 * @param[in] att_status    The attribute return status
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on error
 */
typedef esp_err_t (*esp_ble_conn_cb_t)(const uint8_t *inbuf,
                                       uint16_t inlen,
                                       uint8_t **outbuf,
                                       uint16_t *outlen,
                                       void *priv_data,
                                       uint8_t *att_status);

/** @cond **/
/* BLE CONN_MGR EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_CONN_MGR_EVENTS);
/** @endcond **/

/**
 * @brief Type of event
 *
 */
typedef enum {
    ESP_BLE_CONN_EVENT_UNKNOWN              = 0,    /*!< Unknown event */
    ESP_BLE_CONN_EVENT_STARTED              = 1,    /*!< When BLE connection management start, the event comes */
    ESP_BLE_CONN_EVENT_STOPPED              = 2,    /*!< When BLE connection management stop, the event comes */
    ESP_BLE_CONN_EVENT_CONNECTED            = 3,    /*!< When a new connection was established, the event comes */
    ESP_BLE_CONN_EVENT_DISCONNECTED         = 4,    /*!< When a connection was terminated, the event comes */
    ESP_BLE_CONN_EVENT_DATA_RECEIVE         = 5,    /*!< When notification, indication, or peripheral write data (uuid_fn NULL) is delivered to the application, the event comes */
    ESP_BLE_CONN_EVENT_DISC_COMPLETE        = 6,    /*!< When the ble discover service complete, the event comes */

    ESP_BLE_CONN_EVENT_PERIODIC_REPORT      = 7,    /*!< When the periodic adv report, the event comes */
    ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST   = 8,    /*!< When the periodic sync lost, the event comes */
    ESP_BLE_CONN_EVENT_PERIODIC_SYNC        = 9,    /*!< When the periodic sync, the event comes */
    ESP_BLE_CONN_EVENT_CCCD_UPDATE          = 10,   /*!< When CCCD (Client Characteristic Configuration Descriptor) is written, the event comes */
    ESP_BLE_CONN_EVENT_MTU                  = 11,   /*!< When GATT MTU is negotiated/updated, the event comes */
    ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE    = 12,   /*!< When connection parameters are updated, the event comes */
    ESP_BLE_CONN_EVENT_SCAN_RESULT          = 13,   /*!< When a BLE advertisement is received during scan (central role), the event comes */
    ESP_BLE_CONN_EVENT_ENC_CHANGE           = 14,   /*!< When link encryption state changes (pairing/encryption complete or failed), the event comes */
    ESP_BLE_CONN_EVENT_PASSKEY_ACTION       = 15,   /*!< When passkey input/display/confirm is needed during pairing, the event comes */
} esp_ble_conn_event_t;

/**
 * @brief Type of descriptors
 *
 */
typedef enum {
    ESP_BLE_CONN_DESC_UNKNOWN           = -1,       /*!< Unknown descriptors */
    ESP_BLE_CONN_DESC_EXTENDED          = 0x2900,   /*!< Characteristic Extended Properties */
    ESP_BLE_CONN_DESC_USER              = 0X2901,   /*!< Characteristic User Description */
    ESP_BLE_CONN_DESC_CIENT_CONFIG      = 0X2902,   /*!< Client Characteristic Configuration */
    ESP_BLE_CONN_DESC_SERVER_CONFIG     = 0X2903,   /*!< Server Characteristic Configuration */
    ESP_BLE_CONN_DESC_PRE_FORMAT        = 0X2904,   /*!< Characteristic Presentation Format */
    ESP_BLE_CONN_DESC_AGG_FORMAT        = 0X2905,   /*!< Characteristic Aggregate Format */
    ESP_BLE_CONN_DESC_VALID_RANGE       = 0X2906,   /*!< Valid Range */
    ESP_BLE_CONN_DESC_EXTREPORT         = 0X2907,   /*!< External Report Reference */
    ESP_BLE_CONN_DESC_REPORT            = 0X2908,   /*!< Report Reference */
    ESP_BLE_CONN_DESC_DIGITAL           = 0X2909,   /*!< Number of Digitals */
    ESP_BLE_CONN_DESC_VALUE_TRIGGER     = 0X290A,   /*!< Value Trigger Setting */
    ESP_BLE_CONN_DESC_ENV_SENS_CONFIG   = 0X290B,   /*!< Environmental Sensing Configuration */
    ESP_BLE_CONN_DESC_ENV_SENS_MEASURE  = 0X290C,   /*!< Environmental Sensing Measurement */
    ESP_BLE_CONN_DESC_ENV_TRIGGER       = 0X290D,   /*!< Environmental Sensing Trigger Setting */
    ESP_BLE_CONN_DESC_TIME_TRIGGER      = 0X290E,   /*!< Time Trigger Setting */
    ESP_BLE_CONN_DESC_COMPLETE_BLOCK    = 0X290F,   /*!< Complete BR-EDR Transport Block Data */
} esp_ble_conn_desc_t;

/**
 * @brief Type of UUID
 *
 */
typedef enum {
    BLE_CONN_UUID_TYPE_16 = 16,                     /*!< 16-bit UUID (BT SIG assigned) */
    BLE_CONN_UUID_TYPE_32 = 32,                     /*!< 32-bit UUID (BT SIG assigned) */
    BLE_CONN_UUID_TYPE_128 = 128,                   /*!< 128-bit UUID */
} esp_ble_conn_uuid_type_t;

/**
 * @brief Universal UUID, to be used for any-UUID static allocation
 *
 */
typedef union {
    uint16_t uuid16;                                /*!< 16 bit UUID of the service */
    uint32_t uuid32;                                /*!< 32 bit UUID of the service */
    uint8_t  uuid128[BLE_UUID128_VAL_LEN];          /*!< 128 bit UUID of the service */
} esp_ble_conn_uuid_t;

/**
 * @brief   This structure maps handler required by UUID which are used to BLE characteristics.
 */
typedef struct {
    const char *name;                               /*!< Name of the handler */
    uint8_t type;                                   /*!< Type of the UUID */
    uint16_t flag;                                  /*!< Flag for visiting right */
    esp_ble_conn_uuid_t uuid;                       /*!< Universal UUID, to be used for any-UUID static allocation */
    esp_ble_conn_cb_t  uuid_fn;                     /*!< BLE UUID callback */
} esp_ble_conn_character_t;

/**
 * @brief   This structure maps handler required by UUID which are used to BLE services.
 */
typedef struct {
    uint8_t type;                                   /*!< Type of the UUID */
    uint16_t nu_lookup_count;                       /*!< Number of entries in the Name-UUID lookup table */
    esp_ble_conn_uuid_t uuid;                       /*!< Universal UUID, to be used for any-UUID static allocation */
    esp_ble_conn_character_t *nu_lookup;            /*!< Pointer to the Name-UUID lookup table */
} esp_ble_conn_svc_t;

/**
 * @brief   This structure maps handler required which are used to configure.
 */
typedef struct {
    uint8_t device_name[MAX_BLE_DEVNAME_LEN];      /*!< Local name for peripheral role (name broadcast when advertising) */
    uint8_t remote_name[MAX_BLE_DEVNAME_LEN];      /*!< Remote name for central role (target device name to connect to when scan_cb is NULL) */
    uint8_t broadcast_data[BROADCAST_PARAM_LEN];   /*!< BLE device manufacturer being announce */

    uint16_t    extended_adv_len;                   /*!< Extended advertising data length */
    uint16_t    periodic_adv_len;                   /*!< Periodic advertising data length */
    uint16_t    extended_adv_rsp_len;               /*!< Extended advertising responses data length */

    const char *extended_adv_data;                  /*!< Extended advertising data */
    const char *periodic_adv_data;                  /*!< Periodic advertising data */
    const char *extended_adv_rsp_data;              /*!< Extended advertising responses data */

    uint16_t    include_service_uuid: 1;            /*!< If include service UUID in advertising */
    esp_ble_conn_uuid_type_t adv_uuid_type;         /*!< UUID type: BLE_CONN_UUID_TYPE_16 or BLE_CONN_UUID_TYPE_128. */
    union {
        uint16_t adv_uuid16;                        /*!< 16-bit UUID (when adv_uuid_type is BLE_CONN_UUID_TYPE_16) */
        uint8_t  adv_uuid128[BLE_UUID128_VAL_LEN];  /*!< 128-bit UUID (when adv_uuid_type is BLE_CONN_UUID_TYPE_128) */
    };                                              /*!< Service UUID for advertising */
} esp_ble_conn_config_t;

/**
 * @brief   This structure maps handler required by UUID which are used to data.
 *
 * @note For ESP_BLE_CONN_EVENT_DATA_RECEIVE, the data field is heap-allocated by ble_conn_mgr.
 *       The event handler must call free() on data after use when data is non-NULL.
 *       esp_event_post copies this structure by value only and does not take ownership of data.
 */
typedef struct {
    uint8_t type;                                   /*!< Type of the UUID */
    uint16_t write_conn_id;                         /*!< Connection handle ID; use BLE_CONN_HANDLE_INVALID to use default link */
    esp_ble_conn_uuid_t uuid;                       /*!< Universal UUID, to be used for any-UUID static allocation */
    uint8_t *data;                                  /*!< Data buffer */
    uint16_t data_len;                              /*!< Data size */
} esp_ble_conn_data_t;

/**
 * @brief   This structure represents a periodic advertising sync established during discovery procedure.
 */
typedef struct {
    uint8_t status;                                 /*!< Periodic sync status */
    uint16_t sync_handle;                           /*!< Periodic sync handle */
    uint8_t sid;                                    /*!< Advertising Set ID */
    uint8_t adv_addr[6];                            /*!< Advertiser address */
    uint8_t adv_phy;                                /*!< Advertising PHY*/
    uint16_t per_adv_ival;                          /*!< Periodic advertising interval */
    uint8_t adv_clk_accuracy;                       /*!< Advertiser clock accuracy */
} esp_ble_conn_periodic_sync_t;

/**
 * @brief   This structure represents a periodic advertising report received on established sync.
 */
typedef struct {
    uint16_t sync_handle;                           /*!< Periodic sync handle */
    int8_t tx_power;                                /*!< Advertiser transmit power in dBm (127 if unavailable) */
    int8_t rssi;                                    /*!< Received signal strength indication in dBm (127 if unavailable) */
    uint8_t data_status;                            /*!< Advertising data status*/
    uint8_t data_length;                            /*!< Advertising Data length */
    const uint8_t *data;                            /*!< Advertising data */
} esp_ble_conn_periodic_report_t;

/**
 * @brief   This structure represents a periodic advertising sync lost of established sync.
 */
typedef struct {
    uint16_t sync_handle;                           /*!< Periodic sync handle */
    int reason;                                     /*!< Reason for sync lost */
} esp_ble_conn_periodic_sync_lost_t;

/**
 * @brief   This structure represents CCCD (Client Characteristic Configuration Descriptor) update event.
 */
typedef struct {
    uint16_t conn_handle;                             /*!< Connection handle */
    uint16_t char_handle;                             /*!< Characteristic handle */
    uint8_t uuid_type;                                /*!< UUID type */
    esp_ble_conn_uuid_t uuid;                         /*!< Characteristic UUID */
    bool notify_enable;                                /*!< Notification enabled (bit 0) */
    bool indicate_enable;                             /*!< Indication enabled (bit 1) */
} esp_ble_conn_cccd_update_t;

/**
 * @brief   BLE connection parameters.
 */
typedef struct {
    uint16_t itvl_min;            /*!< Connection event interval minimum. This is in units of 1.25ms. (0x0006 = 7.5ms). Valid range: 0x0006–0x0C80 */
    uint16_t itvl_max;            /*!< Connection event interval maximum. This is in units of 1.25ms. (0x0006 = 7.5ms). Valid range: 0x0006–0x0C80 */
    uint16_t latency;             /*!< Peripheral latency. This is in units of connection events to skip. Valid range: 0x0000–0x01F3 */
    uint16_t supervision_timeout; /*!< Supervision timeout. This is in units of 10ms. (e.g. 400 = 4s). Valid range: 0x000A–0x0C80. Must be larger than the connection interval (itvl_max * 1.25ms) */
    uint16_t min_ce_len;          /*!< Minimum connection event length. Valid range: 0x0000–0x0C80 */
    uint16_t max_ce_len;          /*!< Maximum connection event length. Valid range: 0x0000–0x0C80 */
} esp_ble_conn_params_t;

/**
 * @brief   BLE advertising parameters.
 *          Aligned with LE Set Extended Advertising Parameters (HCI).
 *          Interval units: 0.625ms. e.g. 0x100 = 100ms.
 *          Legacy advertising uses itvl_min, itvl_max, conn_mode, disc_mode;
 *          extended fields are applied when CONFIG_BLE_CONN_MGR_EXTENDED_ADV is enabled.
 */
typedef struct {
    uint8_t  adv_handle;           /*!< Advertising_Handle: instance index; 0 = use default (e.g. 1) */
    uint16_t adv_event_properties; /*!< Advertising_Event_Properties: BIT0=connectable, BIT1=scannable, BIT2=directed, BIT3=high_duty_directed, BIT4=legacy_pdu, BIT5=anonymous, BIT6=include_tx_power, BIT7=scan_req_notif */
    uint16_t itvl_min;            /*!< Primary_Advertising_Interval_Min (0.625ms). Valid 0x0020–0x4000, 0 = default */
    uint16_t itvl_max;            /*!< Primary_Advertising_Interval_Max (0.625ms). Valid 0x0020–0x4000, 0 = default */
    uint8_t  conn_mode;           /*!< Legacy: esp_ble_conn_adv_conn_mode_t */
    uint8_t  disc_mode;           /*!< Legacy: esp_ble_conn_adv_disc_mode_t */
    uint8_t  channel_map;         /*!< Primary_Advertising_Channel_Map: BIT0=ch37, BIT1=ch38, BIT2=ch39. 0 = default (all) */
    uint8_t  own_addr_type;       /*!< Own address type: esp_ble_conn_addr_type_t */
    uint8_t  peer_addr_type;      /*!< Peer address type for directed adv: esp_ble_conn_addr_type_t; 0 = not directed */
    uint8_t  peer_addr[6];        /*!< Peer_Address for directed advertising */
    uint8_t  filter_policy;       /*!< Advertising filter: esp_ble_conn_scan_filt_policy_t */
    int8_t   tx_power;            /*!< Advertising_TX_Power in dBm; 127 = use stack default */
    uint8_t  primary_phy;         /*!< Primary advertising PHY: esp_ble_conn_phy_t */
    uint8_t  secondary_adv_max_skip; /*!< Secondary_Advertising_Max_Skip (0 = send every primary) */
    uint8_t  secondary_phy;       /*!< Secondary advertising PHY: esp_ble_conn_phy_t */
    uint8_t  sid;                 /*!< Advertising_SID (0–15) */
    uint8_t  scan_req_notif;      /*!< Scan_Request_Notification_Enable: 0 = disabled, non-zero = enabled */
    uint8_t  primary_phy_options;   /*!< Primary_Advertising_PHY_Options, 0 = default */
    uint8_t  secondary_phy_options; /*!< Secondary_Advertising_PHY_Options, 0 = default */
    uint8_t  ext_adv_cap;         /*!< Deprecated: use adv_event_properties; applied when adv_event_properties is 0 */
} esp_ble_conn_adv_params_t;

/**
 * @brief   BLE scan parameters.
 *          Aligned with LE Set Scan Parameters / Extended Scan (HCI).
 *          Interval/window units: 0.625ms. e.g. 0x12 = 11.25ms.
 */
typedef struct {
    uint8_t  own_addr_type;       /*!< Own address type: esp_ble_conn_addr_type_t */
    uint8_t  filter_policy;       /*!< Scanning filter: esp_ble_conn_scan_filt_policy_t */
    uint16_t itvl;                /*!< Scan_Interval (0.625ms). 0 = use stack default */
    uint16_t window;              /*!< Scan_Window (0.625ms). 0 = use stack default. Must be <= itvl */
    bool     passive;             /*!< Scan_Type: true = passive scan (no scan requests), false = active */
    bool     filter_duplicates;   /*!< true = filter duplicate advertisements */
    bool     limited;             /*!< true = limited discovery procedure */
    uint8_t  scan_phys;           /*!< Scanning PHYs (bitmask). 0 = standard scan (1M). Reserved and currently ignored by NimBLE backend */
} esp_ble_conn_scan_params_t;

/**
 * @brief   Unified event data for BLE connection manager events.
 *          Use the union member corresponding to the event type (esp_ble_conn_event_t).
 */
typedef struct {
    union {
        struct {
            uint16_t conn_handle;                         /*!< Connection handle */
            uint8_t peer_addr[6];                         /*!< Peer address (big-endian) */
            uint8_t peer_addr_type;                      /*!< Peer address type: esp_ble_conn_addr_type_t */
        } connected;                                     /*!< ESP_BLE_CONN_EVENT_CONNECTED */
        struct {
            uint16_t conn_handle;                         /*!< Connection handle */
            uint16_t reason;                              /*!< Disconnect reason (stack/HCI error code) */
            uint8_t peer_addr[6];                         /*!< Peer address (big-endian) */
            uint8_t peer_addr_type;                       /*!< Peer address type */
        } disconnected;                                   /*!< ESP_BLE_CONN_EVENT_DISCONNECTED */
        esp_ble_conn_data_t data_receive;                 /*!< ESP_BLE_CONN_EVENT_DATA_RECEIVE */
        esp_ble_conn_periodic_report_t periodic_report;   /*!< ESP_BLE_CONN_EVENT_PERIODIC_REPORT */
        esp_ble_conn_periodic_sync_lost_t periodic_sync_lost; /*!< ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST */
        esp_ble_conn_periodic_sync_t periodic_sync;       /*!< ESP_BLE_CONN_EVENT_PERIODIC_SYNC */
        esp_ble_conn_cccd_update_t cccd_update;           /*!< ESP_BLE_CONN_EVENT_CCCD_UPDATE */
        struct {
            uint16_t conn_handle;                         /*!< Connection handle */
            uint16_t channel_id;                          /*!< GATT channel ID (0 for ATT) */
            uint16_t mtu;                                 /*!< Negotiated MTU value */
        } mtu_update;                                     /*!< ESP_BLE_CONN_EVENT_MTU */
        struct {
            uint16_t conn_handle;                         /*!< Connection handle */
            int status;                                   /*!< Update status (0 on success, non-zero on failure) */
            esp_ble_conn_params_t params;                 /*!< Updated connection parameters */
        } conn_param_update;                               /*!< ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE */
        esp_ble_conn_scan_result_t scan_result;           /*!< ESP_BLE_CONN_EVENT_SCAN_RESULT */
        struct {
            uint16_t conn_handle;                         /*!< Connection handle */
            int status;                                   /*!< 0 on success, non-zero on failure */
            bool encrypted;                               /*!< Link is encrypted */
            bool authenticated;                           /*!< Link is authenticated */
            uint8_t peer_addr[6];                          /*!< Peer address */
            uint8_t peer_addr_type;                       /*!< Peer address type */
        } enc_change;                                     /*!< ESP_BLE_CONN_EVENT_ENC_CHANGE */
        struct {
            uint16_t conn_handle;                         /*!< Connection handle */
            esp_ble_conn_sm_action_t action;               /*!< Passkey action type, see esp_ble_conn_sm_action_t */
            uint32_t passkey;                             /*!< Passkey to display (DISP), or for user to enter (INPUT), or to compare (NUMCMP) */
        } passkey_action;                                 /*!< ESP_BLE_CONN_EVENT_PASSKEY_ACTION */
    };
} esp_ble_conn_event_data_t;

#define BLE_UUID_CMP(type, src, dst) \
            ((type == BLE_CONN_UUID_TYPE_16) && (src.uuid16 == dst.uuid16)) ||  \
            ((type == BLE_CONN_UUID_TYPE_32) && (src.uuid32 == dst.uuid32)) ||  \
            ((type == BLE_CONN_UUID_TYPE_128) && (!memcmp(src.uuid128, dst.uuid128, BLE_UUID128_VAL_LEN)))

#define BLE_UUID_TYPE(type) \
            ((type != BLE_CONN_UUID_TYPE_16) && (type != BLE_CONN_UUID_TYPE_32) && (type != BLE_CONN_UUID_TYPE_128))

#ifndef MIN
#define MIN(a, b)                                   (((a) < (b)) ? (a) : (b))
#endif

/**
 * @brief   Initialization *BLE* connection management based on the configuration
 *          This function must be the first function to call,
 *          This call MUST have a corresponding call to esp_ble_conn_deinit when the operation is complete.
 *
 * @param[in]   config The configurations, see `esp_ble_conn_config_t`.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_conn_init(esp_ble_conn_config_t *config);

/**
 * @brief   Deinitialization *BLE* connection management
 *          This function must be the last function to call,
 *          It is the opposite of the esp_ble_conn_init function.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong deinitialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_conn_deinit(void);

/**
 * @brief   Starts *BLE* session
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong start
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_start(void);

/**
 * @brief   Stop *BLE* session
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong stop
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_stop(void);

/**
 * @brief   Legacy API to set preferred MTU.
 *
 * This API is kept for backward compatibility and is a no-op in multi-connection
 * implementation. Use esp_ble_conn_mtu_update(conn_handle, mtu) instead.
 *
 * @param[in]  mtu The maximum transmission unit value to update
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong update
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_set_mtu(uint16_t mtu);

/**
 * @brief This api is typically used to connect actively
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if BLE connection manager is not initialized
 *  - ESP_ERR_TIMEOUT if connect event is not received within wait timeout
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_connect(void);

/**
 * @brief This api is typically used to disconnect actively
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if BLE connection manager is not initialized
 *  - ESP_ERR_NOT_FOUND if no active connection exists
 *  - ESP_ERR_TIMEOUT if disconnect event is not received within wait timeout
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_disconnect(void);

/**
 * @brief   Initiate BLE connection to a peer (Central role).
 *
 * @param[in] peer_addr      Peer BLE address (6 bytes)
 * @param[in] peer_addr_type  Peer address type: esp_ble_conn_addr_type_t
 *
 * @note Only one outgoing connection attempt is allowed at a time.
 *       Returns ESP_ERR_INVALID_STATE if a connection attempt is already in progress.
 *
 * @return
 *  - ESP_OK on success (connection initiated; result via ESP_BLE_CONN_EVENT_CONNECTED/DISCONNECTED)
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_INVALID_STATE if max connections reached, peer already connected/pending, or another attempt in progress
 *  - ESP_ERR_NOT_SUPPORTED if Central role is not enabled
 *  - ESP_FAIL if connection initiation fails
 */
esp_err_t esp_ble_conn_connect_to_addr(const uint8_t peer_addr[6], uint8_t peer_addr_type);

/**
 * @brief   Disconnect a BLE connection by handle.
 *
 * @param[in] conn_handle Connection handle to terminate (valid range 0x0000–0x0EFF)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if conn_handle is BLE_CONN_HANDLE_INVALID or out of range
 *  - ESP_FAIL if disconnect fails
 */
esp_err_t esp_ble_conn_disconnect_by_handle(uint16_t conn_handle);

/**
 * @brief   Get current (default) connection handle (read-only).
 *
 * In multi-connection mode this returns the first/primary connection handle, or BLE_CONN_HANDLE_INVALID if none.
 *
 * @param[out] out_conn_handle  Pointer to store the handle. Set to BLE_CONN_HANDLE_INVALID when not connected.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if out_conn_handle is NULL
 */
esp_err_t esp_ble_conn_get_conn_handle(uint16_t *out_conn_handle);

/**
 * @brief   Get connection handle by peer address.
 *
 * Use this in multi-connection scenarios to resolve a peer address to its connection handle (valid range 0x0000–0x0EFF).
 *
 * @param[in]  peer_addr       Peer BLE address (6 bytes)
 * @param[in]  peer_addr_type  Peer address type: esp_ble_conn_addr_type_t
 * @param[out] out_conn_handle Pointer to store the connection handle for that peer
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if peer_addr or out_conn_handle is NULL
 *  - ESP_ERR_NOT_FOUND if no connection exists for the given peer address
 */
esp_err_t esp_ble_conn_get_conn_handle_by_addr(const uint8_t peer_addr[6], uint8_t peer_addr_type,
                                               uint16_t *out_conn_handle);

/**
 * @brief   Get current negotiated GATT MTU (read-only).
 *
 * @param[out] out_mtu  Pointer to store current MTU. Defaults to 23 if unknown.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if out_mtu is NULL
 */
esp_err_t esp_ble_conn_get_mtu(uint16_t *out_mtu);

/**
 * @brief   Get negotiated GATT MTU by connection handle.
 *
 * @param[in]  conn_handle Connection handle (valid range 0x0000–0x0EFF; BLE_CONN_HANDLE_INVALID is invalid)
 * @param[out] out_mtu     Pointer to store MTU
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (e.g. conn_handle out of range)
 *  - ESP_ERR_NOT_FOUND if connection does not exist
 */
esp_err_t esp_ble_conn_get_mtu_by_handle(uint16_t conn_handle, uint16_t *out_mtu);

/**
 * @brief   Get current peer address (read-only).
 *
 * @param[out] out_addr  Pointer to store peer address (6 bytes).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if out_addr is NULL
 *  - ESP_ERR_NOT_FOUND if not connected
 */
esp_err_t esp_ble_conn_get_peer_addr(uint8_t out_addr[6]);

/**
 * @brief   Get peer address by connection handle.
 *
 * @param[in]  conn_handle        Connection handle
 * @param[out] out_peer_addr      Pointer to store peer address (6 bytes)
 * @param[out] out_peer_addr_type Pointer to store peer address type (can be NULL)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_FOUND if connection does not exist
 */
esp_err_t esp_ble_conn_get_peer_addr_by_handle(uint16_t conn_handle, uint8_t out_peer_addr[6],
                                               uint8_t *out_peer_addr_type);

/**
 * @brief   Find connection handle by peer address.
 *
 * @param[in]  peer_addr      Peer address (6 bytes)
 * @param[in]  peer_addr_type Peer address type
 * @param[out] out_conn_handle Pointer to store matched connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_FOUND if no matched connection exists
 */
esp_err_t esp_ble_conn_find_conn_handle_by_peer_addr(const uint8_t peer_addr[6], uint8_t peer_addr_type,
                                                     uint16_t *out_conn_handle);

/**
 * @brief   Find peer address by connection handle (deprecated).
 *
 * @deprecated Use esp_ble_conn_get_peer_addr_by_handle() instead.
 *
 * @param[in]  conn_handle        Connection handle
 * @param[out] out_peer_addr      Output peer address (6 bytes)
 * @param[out] out_peer_addr_type Output peer address type (can be NULL)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_FOUND if the connection handle does not exist
 */
esp_err_t esp_ble_conn_find_peer_addr_by_conn_handle(uint16_t conn_handle, uint8_t out_peer_addr[6],
                                                     uint8_t *out_peer_addr_type)
__attribute__((deprecated("use esp_ble_conn_get_peer_addr_by_handle")));

/**
 * @brief   Get last disconnect reason (read-only).
 *
 * @param[out] out_reason  Pointer to store last disconnect reason.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if out_reason is NULL
 *  - ESP_ERR_NOT_FOUND if no disconnect reason is available
 */
esp_err_t esp_ble_conn_get_disconnect_reason(uint16_t *out_reason);

/**
 * @brief   Get disconnect reason by connection handle.
 *
 * @param[in]  conn_handle Connection handle
 * @param[out] out_reason  Pointer to store disconnect reason
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_FOUND if reason is unavailable for the connection
 */
esp_err_t esp_ble_conn_get_disconnect_reason_by_handle(uint16_t conn_handle, uint16_t *out_reason);

/**
 * @brief   Request connection parameter update.
 *
 * @param[in] conn_handle  Connection handle (use esp_ble_conn_get_conn_handle to query)
 * @param[in] params       Desired connection parameters
 *                         (min_ce_len and max_ce_len are forwarded to NimBLE).
 *
 * @return
 *  - ESP_OK on success (request accepted)
 *  - ESP_ERR_INVALID_ARG on wrong parameters
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_update_params(uint16_t conn_handle, const esp_ble_conn_params_t *params);

/**
 * @brief   Get current LE PHY for a connection.
 *
 * @param[in]  conn_handle   Connection handle (valid range 0x0000–0x0EFF).
 * @param[out] out_tx_phy    Pointer to store TX PHY (esp_ble_conn_phy_t: 1=1M, 2=2M, 3=Coded). Can be NULL.
 * @param[out] out_rx_phy    Pointer to store RX PHY (esp_ble_conn_phy_t). Can be NULL.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if conn_handle is invalid or both out_tx_phy and out_rx_phy are NULL
 *  - ESP_ERR_NOT_FOUND if connection does not exist
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_get_phy(uint16_t conn_handle, uint8_t *out_tx_phy, uint8_t *out_rx_phy);

/**
 * @brief   Set preferred LE PHY for a connection.
 *
 * The controller may not apply the change if the peer does not support the requested PHY.
 *
 * @param[in] conn_handle    Connection handle (valid range 0x0000–0x0EFF).
 * @param[in] tx_phys_mask   Preferred TX PHY mask: ESP_BLE_CONN_PHY_MASK_1M, _2M, _CODED, or _ALL (or combination).
 * @param[in] rx_phys_mask   Preferred RX PHY mask (same as tx_phys_mask).
 * @param[in] phy_opts       Coded PHY option when using CODED: ESP_BLE_CONN_PHY_CODED_OPT_ANY, _S2, or _S8; otherwise 0.
 *
 * @return
 *  - ESP_OK on success (request accepted)
 *  - ESP_ERR_INVALID_ARG if conn_handle is invalid or out of range
 *  - ESP_ERR_NOT_FOUND if connection does not exist
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_set_preferred_phy(uint16_t conn_handle, uint8_t tx_phys_mask, uint8_t rx_phys_mask, uint16_t phy_opts);

/**
 * @brief   Set preferred data length for a connection (LE Data Length Extension).
 *
 * Sends LE Set Data Length command to the controller. The controller negotiates with the peer
 * asynchronously. Per BLE spec: tx_octets 27–251 bytes, tx_time 328–17040 microseconds.
 *
 * @param[in] conn_handle  Connection handle (valid range 0x0000–0x0EFF).
 * @param[in] tx_octets    Preferred TX payload size in bytes (27–251).
 * @param[in] tx_time      Preferred TX time in microseconds (328–17040).
 *
 * @return
 *  - ESP_OK on success (command accepted)
 *  - ESP_ERR_INVALID_ARG if conn_handle or parameters are out of range
 *  - ESP_ERR_NOT_FOUND if connection does not exist
 *  - ESP_ERR_NOT_SUPPORTED if Data Length Extension is not supported
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_set_data_len(uint16_t conn_handle, uint16_t tx_octets, uint16_t tx_time);

/**
 * @brief   Configure advertising parameters.
 *
 * @param[in] params  Advertising parameters (NULL to use defaults)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if BLE connection manager is not initialized
 */
esp_err_t esp_ble_conn_adv_params_set(const esp_ble_conn_adv_params_t *params);

/**
 * @brief   Set advertising data (legacy or extended).
 *
 * @param[in] data  Raw advertising data (format: length-byte + type + value per AD structure). Can be NULL to clear.
 * @param[in] len   Data length in bytes. Max: @c ESP_BLE_CONN_ADV_DATA_MAX_LEN (31) for legacy;
 *                  larger when extended advertising is enabled (platform-dependent).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if BLE connection manager is not initialized
 *  - ESP_ERR_INVALID_ARG if len exceeds maximum or data is NULL while len > 0
 *  - ESP_ERR_NO_MEM on allocation failure
 */
esp_err_t esp_ble_conn_adv_data_set(const uint8_t *data, uint16_t len);

/**
 * @brief   Set scan response data (legacy or extended).
 *
 * @param[in] data  Raw scan response data (format: length-byte + type + value per AD structure). Can be NULL to clear.
 * @param[in] len   Data length in bytes. Max: @c ESP_BLE_CONN_ADV_DATA_MAX_LEN (31) for legacy;
 *                  larger when extended advertising is enabled (platform-dependent).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if BLE connection manager is not initialized
 *  - ESP_ERR_INVALID_ARG if len exceeds maximum or data is NULL while len > 0
 *  - ESP_ERR_NO_MEM on allocation failure
 */
esp_err_t esp_ble_conn_scan_rsp_data_set(const uint8_t *data, uint16_t len);

/**
 * @brief   Set periodic advertising data (extended advertising only).
 *
 * @param[in] data  Raw periodic advertising data. Can be NULL to clear.
 * @param[in] len   Data length in bytes (platform-dependent max when CONFIG_BLE_CONN_MGR_PERIODIC_ADV is set).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if BLE connection manager is not initialized
 *  - ESP_ERR_INVALID_ARG if len exceeds maximum or data is NULL while len > 0
 *  - ESP_ERR_NO_MEM on allocation failure
 *  - ESP_ERR_NOT_SUPPORTED if periodic advertising is not enabled
 */
esp_err_t esp_ble_conn_periodic_adv_data_set(const uint8_t *data, uint16_t len);

/**
 * @brief   Start advertising (peripheral role).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if not initialized or not in peripheral role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_adv_start(void);

/**
 * @brief   Stop advertising (peripheral role).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if not initialized
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_adv_stop(void);

/**
 * @brief   Configure scan parameters.
 *
 * @param[in] params  Scan parameters (NULL to use defaults)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if BLE connection manager is not initialized
 */
esp_err_t esp_ble_conn_scan_params_set(const esp_ble_conn_scan_params_t *params);

/**
 * @brief   Start scanning (central role).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if not initialized or not in central role
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_scan_start(void);

/**
 * @brief   Stop ongoing BLE scan (central role).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if scan is not active
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_scan_stop(void);

/**
 * @brief   Initiate MTU exchange on a connection.
 *
 * Sets preferred MTU and initiates exchange (central) or sets preferred for future (peripheral).
 *
 * @param[in] conn_handle  Connection handle
 * @param[in] mtu          Desired MTU (BLE_ATT_MTU_DFLT to BLE_ATT_MTU_MAX, typically 512)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_mtu_update(uint16_t conn_handle, uint16_t mtu);

/**
 * @brief   Set local passkey for pairing (DISP mode).
 *
 * When the device displays a passkey during pairing (action DISP), this key
 * is used and auto-injected. Set to 0 to disable (application handles via
 * esp_ble_conn_passkey_reply).
 *
 * @param[in] passkey  6-digit passkey (0-999999); 0 to clear
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if BLE connection manager is not initialized
 */
esp_err_t esp_ble_conn_set_local_passkey(uint32_t passkey);

/**
 * @brief   Initiate BLE security procedure.
 *
 * Call this to start pairing. When ESP_BLE_CONN_EVENT_PASSKEY_ACTION occurs,
 * use esp_ble_conn_passkey_reply() or esp_ble_conn_numcmp_reply() to respond.
 *
 * @param[in] conn_handle  Connection handle
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if conn_handle is BLE_CONN_HANDLE_INVALID or out of range
 *  - ESP_ERR_NOT_SUPPORTED if stack SMP is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_security_initiate(uint16_t conn_handle);

/**
 * @brief   Reply to passkey action.
 *
 * Call when ESP_BLE_CONN_EVENT_PASSKEY_ACTION has action INPUT.
 * Pass the passkey entered by the user.
 *
 * @param[in] conn_handle  Connection handle
 * @param[in] passkey      6-digit passkey (0-999999)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_SUPPORTED if NimBLE SMP is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_passkey_reply(uint16_t conn_handle, uint32_t passkey);

/**
 * @brief   Reply to numeric comparison (NUMCMP).
 *
 * Call when ESP_BLE_CONN_EVENT_PASSKEY_ACTION has action NUMCMP.
 *
 * @param[in] conn_handle  Connection handle
 * @param[in] accept       true to accept, false to reject
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_SUPPORTED if NimBLE SMP is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_numcmp_reply(uint16_t conn_handle, bool accept);

/**
 * @brief   Create an L2CAP CoC server
 *
 * @param[in] psm     Protocol/Service Multiplexer to listen on (must be in range 0x0001-0x00FF for LE)
 * @param[in] mtu     Maximum SDU size to use for the server (must be in range 23-512 bytes)
 * @param[in] cb      L2CAP CoC event callback
 * @param[in] cb_arg  Callback argument
 *
 * @note The callback context is persistent for this PSM and is reused across
 *       incoming connections. It is not auto-freed until the L2CAP CoC memory
 *       pool is released.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (NULL callback, PSM out of range 0x0001-0x00FF, or MTU out of range)
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_create_server(uint16_t psm, uint16_t mtu,
                                               esp_ble_conn_l2cap_coc_event_cb_t cb,
                                               void *cb_arg);

/**
 * @brief   Initiate an L2CAP CoC connection
 *
 * @param[in] conn_handle  Connection handle (valid range 0x0000–0x0EFF; BLE_CONN_HANDLE_INVALID is invalid)
 * @param[in] psm          Protocol/Service Multiplexer to connect to (must be in range 0x0001-0x00FF for LE)
 * @param[in] mtu          Maximum SDU size to use for the connection (must be in range 23-512 bytes)
 * @param[in] sdu_size     Receive buffer size for the first SDU (must be in range 23-65535 bytes).
 *                         This size is used to allocate the initial RX buffer for the channel.
 *                         Applications typically set it to the expected peer CoC MTU.
 * @param[in] cb           L2CAP CoC event callback
 * @param[in] cb_arg       Callback argument
 *
 * @note The callback context is auto-freed if the connection attempt fails
 *       (ESP_BLE_CONN_L2CAP_COC_EVENT_CONNECTED with non-zero status) and after
 *       ESP_BLE_CONN_L2CAP_COC_EVENT_DISCONNECTED.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (NULL callback, conn_handle/PSM/sdu_size out of range, or MTU out of range)
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_connect(uint16_t conn_handle, uint16_t psm, uint16_t mtu,
                                         uint16_t sdu_size,
                                         esp_ble_conn_l2cap_coc_event_cb_t cb,
                                         void *cb_arg);

/**
 * @brief   Send an SDU over an L2CAP CoC channel
 *
 * @param[in] chan  L2CAP CoC channel
 * @param[in] sdu   SDU to send (data buffer is not owned by the stack, can be freed
 *                  immediately after this function returns)
 *
 * @note Flow control: If the channel is stalled (buffer full), this function returns
 *       ESP_ERR_NOT_FINISHED. The caller should wait for ESP_BLE_CONN_L2CAP_COC_EVENT_TX_UNSTALLED
 *       event before retrying the send operation.
 *
 * @note Memory pool: If mbuf allocation fails (ESP_ERR_NO_MEM), it indicates the memory
 *       pool is exhausted. The caller should wait and retry later, or check if there
 *       are pending operations that need to complete.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (NULL channel, NULL sdu, invalid data pointer, or zero length)
 *  - ESP_ERR_INVALID_STATE if memory pool not initialized or channel not connected
 *  - ESP_ERR_NO_MEM if mbuf allocation failed (memory pool exhausted)
 *  - ESP_ERR_NOT_FINISHED if channel is stalled or send would block (wait for TX_UNSTALLED event)
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_send(esp_ble_conn_l2cap_coc_chan_t chan,
                                      const esp_ble_conn_l2cap_coc_sdu_t *sdu);

/**
 * @brief   Provide a receive buffer for an L2CAP CoC channel
 *
 * @param[in] chan      L2CAP CoC channel
 * @param[in] sdu_size  Receive buffer size for next SDU
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NO_MEM if memory pool not initialized or allocation failed
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_recv_ready(esp_ble_conn_l2cap_coc_chan_t chan,
                                            uint16_t sdu_size);

/**
 * @brief   Disconnect an L2CAP CoC channel
 *
 * @param[in] chan  L2CAP CoC channel
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_disconnect(esp_ble_conn_l2cap_coc_chan_t chan);

/**
 * @brief   Get L2CAP CoC channel information
 *
 * @param[in]  chan      L2CAP CoC channel
 * @param[out] chan_info Channel info output
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 *  - ESP_ERR_NOT_SUPPORTED if L2CAP CoC is disabled
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_l2cap_coc_get_chan_info(esp_ble_conn_l2cap_coc_chan_t chan,
                                               esp_ble_conn_l2cap_coc_chan_info_t *chan_info);

/**
 * @brief   Register scan callback for BLE central role
 *
 * @param[in] cb      Scan callback (can be NULL to disable)
 * @param[in] cb_arg  Callback argument
 *
 * @note When the callback returns true, a connection is initiated to that peer.
 *       Only one outgoing connection attempt is allowed at a time. If a connection
 *       attempt is already in progress, further scan results that would trigger
 *       connect are skipped until the current attempt completes (success or fail).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 */
esp_err_t esp_ble_conn_register_scan_callback(esp_ble_conn_scan_cb_t cb, void *cb_arg);

/**
 * @brief   Parse AD type data from raw advertising data
 *
 * @param[in]  adv_data   Raw advertising data
 * @param[in]  adv_len    Raw advertising data length
 * @param[in]  ad_type    Advertising data type
 * @param[out] out_data   Output data pointer (points into adv_data)
 * @param[out] out_len    Output data length
 *
 * @return
 *  - ESP_OK if AD type is found
 *  - ESP_ERR_NOT_FOUND if AD type is not present
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 */
esp_err_t esp_ble_conn_parse_adv_data(const uint8_t *adv_data, uint8_t adv_len, uint8_t ad_type,
                                      const uint8_t **out_data, uint8_t *out_len);

/**
 * @brief This api is typically used to notify actively
 *
 * @param[in]  inbuff The pointer to store notify data.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong notify
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_notify(const esp_ble_conn_data_t *inbuff);

/**
 * @brief Send notification/indication to a specific connection.
 *
 * @param[in] conn_handle Connection handle (valid range 0x0000–0x0EFF; BLE_CONN_HANDLE_INVALID is invalid).
 * @param[in] inbuff      Notify payload (characteristic UUID and data).
 *
 * @note The given `conn_handle` is the effective target; `inbuff->write_conn_id` is ignored. Use this API for
 *       multi-connection when sending to a specific link.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (e.g. conn_handle out of range)
 *  - ESP_ERR_NO_MEM if system resources are insufficient
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_notify_by_handle(uint16_t conn_handle, const esp_ble_conn_data_t *inbuff);

/**
 * @brief This api is typically used to read actively
 *
 * @param[in]  outbuf The pointer to store read data.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong read
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_read(esp_ble_conn_data_t *outbuf);

/**
 * @brief Read characteristic on a specific connection.
 *
 * @param[in]     conn_handle Connection handle (valid range 0x0000–0x0EFF; BLE_CONN_HANDLE_INVALID is invalid).
 * @param[in,out] outbuf      Read request/response buffer (UUID and optional write_conn_id; response in outbuf).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (e.g. conn_handle out of range)
 *  - ESP_ERR_TIMEOUT if read completion is not received within wait timeout
 *  - ESP_FAIL or stack-specific error code on read failure
 */
esp_err_t esp_ble_conn_read_by_handle(uint16_t conn_handle, esp_ble_conn_data_t *outbuf);

/**
 * @brief This api is typically used to write actively
 *
 * @param[in]  inbuff The pointer to store write data.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong write
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_write(const esp_ble_conn_data_t *inbuff);

/**
 * @brief Write characteristic on a specific connection.
 *
 * @param[in] conn_handle Connection handle (valid range 0x0000–0x0EFF; BLE_CONN_HANDLE_INVALID is invalid).
 * @param[in] inbuff      Write payload (characteristic UUID and data).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (e.g. conn_handle out of range)
 *  - ESP_ERR_NO_MEM if system resources are insufficient
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_write_by_handle(uint16_t conn_handle, const esp_ble_conn_data_t *inbuff);

/**
 * @brief This api is typically used to subscribe actively
 *
 * @param[in]  desc   The declarations of descriptors
 * @param[in]  inbuff The pointer to store subscribe data.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong subscribe
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_subscribe(esp_ble_conn_desc_t desc, const esp_ble_conn_data_t *inbuff);

/**
 * @brief Subscribe to notifications/indications on a specific connection.
 *
 * @param[in] conn_handle Connection handle (valid range 0x0000–0x0EFF; BLE_CONN_HANDLE_INVALID is invalid).
 * @param[in] desc        Descriptor type (e.g. ESP_BLE_CONN_DESC_CIENT_CONFIG for CCCD).
 * @param[in] inbuff      Subscribe payload (characteristic UUID and enable flags).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters (e.g. conn_handle out of range)
 *  - ESP_ERR_NO_MEM if system resources are insufficient
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_subscribe_by_handle(uint16_t conn_handle, esp_ble_conn_desc_t desc,
                                           const esp_ble_conn_data_t *inbuff);

/**
 * @brief This api is typically used to add service actively
 *
 * @param[in]  svc The pointer to store service.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong add service
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_add_svc(const esp_ble_conn_svc_t *svc);

/**
 * @brief This api is typically used to remove service actively
 *
 * @param[in]  svc The pointer to store service.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong remove service
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_remove_svc(const esp_ble_conn_svc_t *svc);

#ifdef __cplusplus
}
#endif
