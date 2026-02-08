/*
 * SPDX-FileCopyrightText: 2019-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_event.h>
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
    ESP_BLE_CONN_EVENT_DATA_RECEIVE         = 5,    /*!< When receive a notification or indication data, the event comes */
    ESP_BLE_CONN_EVENT_DISC_COMPLETE        = 6,    /*!< When the ble discover service complete, the event comes */

    ESP_BLE_CONN_EVENT_PERIODIC_REPORT      = 7,    /*!< When the periodic adv report, the event comes */
    ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST   = 8,    /*!< When the periodic sync lost, the event comes */
    ESP_BLE_CONN_EVENT_PERIODIC_SYNC        = 9,    /*!< When the periodic sync, the event comes */
    ESP_BLE_CONN_EVENT_CCCD_UPDATE          = 10   /*!< When CCCD (Client Characteristic Configuration Descriptor) is written, the event comes */
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
    uint8_t device_name[MAX_BLE_DEVNAME_LEN];       /*!< BLE device name being broadcast */
    uint8_t broadcast_data[BROADCAST_PARAM_LEN];    /*!< BLE device manufacturer being announce */

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
 */
typedef struct {
    uint8_t type;                                   /*!< Type of the UUID */
    uint16_t write_conn_id;                         /*!< Connection handle ID */
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
    uint16_t min_ce_len;          /*!< Minimum connection event length. This is in units of 0.625ms. 0 if unused. */
    uint16_t max_ce_len;          /*!< Maximum connection event length. This is in units of 0.625ms. 0 if unused. */
} esp_ble_conn_params_t;

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
 * @brief   This api is typically used to update maximum transmission unit value
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
 *  - ESP_ERR_INVALID_ARG on wrong connect
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_connect(void);

/**
 * @brief This api is typically used to disconnect actively
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on wrong disconnect
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_disconnect(void);

/**
 * @brief   Get current connection handle (read-only).
 *
 * @param[out] out_conn_handle  Pointer to store current connection handle. 0 if not connected.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if out_conn_handle is NULL
 */
esp_err_t esp_ble_conn_get_conn_handle(uint16_t *out_conn_handle);

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
 * @brief   Request connection parameter update.
 *
 * @param[in] conn_handle  Connection handle (use esp_ble_conn_get_conn_handle to query)
 * @param[in] params       Desired connection parameters
 *
 * @return
 *  - ESP_OK on success (request accepted)
 *  - ESP_ERR_INVALID_ARG on wrong parameters
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_update_params(uint16_t conn_handle, const esp_ble_conn_params_t *params);

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
 * @param[in] conn_handle  Connection handle (must be in range 0x0001-0x0EFF)
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
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG on invalid parameters
 */
esp_err_t esp_ble_conn_register_scan_callback(esp_ble_conn_scan_cb_t cb, void *cb_arg);

/**
 * @brief   Stop ongoing BLE scan (central role)
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if scan is not active
 *  - ESP_FAIL on other error
 */
esp_err_t esp_ble_conn_scan_stop(void);

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
