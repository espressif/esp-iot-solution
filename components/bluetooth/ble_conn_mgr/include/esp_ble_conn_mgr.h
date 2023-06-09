/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

/**
 * @brief This is type of function that will handle the registered characteristic
 *
 * @param[in] inbuf         The pointer to store data: read operation if NULL or write operation if not NULL
 * @param[in] inlen         The store data length
 * @param[out] outbuf       Variable to store data, it'll free by connection management component
 * @param[out] outlen       Variable to store data length
 * @param[in] priv_data     Private data context
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
                                       void *priv_data);

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
    ESP_BLE_CONN_EVENT_PERIODIC_SYNC        = 9     /*!< When the periodic sync, the event comes */
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
    uint8_t flag;                                   /*!< Flag for visiting right */
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
