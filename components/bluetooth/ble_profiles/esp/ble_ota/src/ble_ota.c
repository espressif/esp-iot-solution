/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_idf_version.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "ble_ota.h"

#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#endif

#define TAG  "ESP_BLE_OTA"
#define DEVICE_NAME  "ESP-C919"

#ifdef CONFIG_BT_BLUEDROID_ENABLED

#define OTA_PROFILE_NUM           2
#define OTA_PROFILE_APP_IDX       0
#define DIS_PROFILE_APP_IDX       1
#define BUF_LENGTH                4098

#define BLE_OTA_MAX_CHAR_VAL_LEN  600

#define BLE_OTA_START_CMD         0x0001
#define BLE_OTA_STOP_CMD          0x0002
#define BLE_OTA_ACK_CMD           0x0003

#define CHAR_DECLARATION_SIZE    (sizeof(uint8_t))

typedef enum {
    BLE_OTA_CMD_ACK = 0,        /*!< Command Ack */
    BLE_OTA_FW_ACK  = 1,        /*!< Firmware Ack */
} ble_ota_ack_type_t;

typedef enum {
    BLE_OTA_CMD_SUCCESS = 0x0000,       /*!< Success Ack */
    BLE_OTA_REJECT      = 0x0001,       /*!< Reject Cmd Ack */
} ble_ota_cmd_ack_status_t;

typedef enum {
    BLE_OTA_FW_SUCCESS = 0x0000,        /*!< Success */
    BLE_OTA_FW_CRC_ERR = 0x0001,        /*!< CRC error */
    BLE_OTA_FW_IND_ERR = 0x0002,        /*!< Sector Index error*/
    BLE_OTA_FW_LEN_ERR = 0x0003,        /*!< Payload length error*/
} ble_ota_fw_ack_statuc_t;

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t conn_id;
    uint16_t mtu_size;
};

#ifdef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#define EXT_ADV_HANDLE                            0
#define NUM_EXT_ADV_SET                           1
#define EXT_ADV_DURATION                          0
#define EXT_ADV_MAX_EVENTS                        0

static uint8_t ext_ota_adv_data[] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0x18, 0x80, //UUID
    0x09, 0x09, 0x45, 0x53, 0x50, 0x2D, 0x43, 0x39, 0x31, 0x39, //ESP-C919
    0x0d, 0xff, 0xe5, 0x02, 0x01, 0x01, 0x27, 0x95, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};

static esp_ble_gap_ext_adv_t ext_adv[1] = {
    [0] = {EXT_ADV_HANDLE, EXT_ADV_DURATION, EXT_ADV_MAX_EVENTS},
};

esp_ble_gap_ext_adv_params_t ext_ota_adv_params = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE,
    .interval_min = 0x20,
    .interval_max = 0x20,
    .channel_map = ADV_CHNL_ALL,
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_2M,
    .sid = 0,
    .scan_req_notif = false,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
};
#else
static esp_ble_adv_params_t ota_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static const uint8_t ota_adv_data[31] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0x18, 0x80, //UUID
    0x09, 0x09, 0x45, 0x53, 0x50, 0x2D, 0x43, 0x39, 0x31, 0x39, //ESP-C919
    0x0d, 0xff, 0xe5, 0x02, 0x01, 0x01, 0x27, 0x95, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};

static const uint8_t ota_scan_rsp_data[31] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0x18, 0x80, //UUID
    0x09, 0x09, 0x45, 0x53, 0x50, 0x2D, 0x43, 0x39, 0x31, 0x39, //ESP-C919
    0x0d, 0xff, 0xe5, 0x02, 0x01, 0x01, 0x27, 0x95, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};
#endif

esp_ble_ota_callback_funs_t ota_cb_fun_t = {
    .recv_fw_cb = NULL
};

esp_ble_ota_notification_check_t ota_notification = {
    .recv_fw_ntf_enable = false,
    .process_bar_ntf_enable = false,
    .command_ntf_enable = false,
    .customer_ntf_enable = false,
};

static void gatts_ota_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_dis_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/**
 * @brief           This function is called to send notification to remote device
 *
 * @param[in]       ota_char: the characteristic index which to be send
 * @param[in]       value: the pointer to the send value
 * @param[in]       length: the value length
 *
 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 */
esp_err_t esp_ble_ota_notification_data(esp_ble_ota_char_t ota_char, uint8_t *value, uint8_t length);

/**
 * @brief           This function is called to set the attribute value by the application
 *
 * @param[in]       ota_char: the characteristic index which to be set
 * @param[in]       value: the pointer to the attribute value
 * @param[in]       length: the value length
 *
 * @return
 *                  - ESP_OK : success
 *                  - other  : failed
 */
esp_err_t esp_ble_ota_set_value(esp_ble_ota_char_t ota_char, uint8_t *value, uint8_t length);

static struct gatts_profile_inst ota_profile_tab[OTA_PROFILE_NUM] = {
    [OTA_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_ota_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        .conn_id  = 0xff,
        .mtu_size = 23,
    },
    [DIS_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_dis_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        .conn_id  = 0xff,
        .mtu_size = 23,
    },
};

static bool start_ota = false;
static unsigned int ota_total_len = 0;
static unsigned int cur_sector = 0;
static unsigned int cur_packet = 0;
static uint8_t *fw_buf = NULL;
static unsigned int fw_buf_offset = 0;
static uint8_t *temp_prep_write_buf = NULL;
static unsigned int temp_buf_len = 0;

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_indicate = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE;
static const uint8_t char_prop_write_indicate = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_INDICATE;

static const uint16_t BLE_OTA_SERVICE_UUID              = 0x8018;

static const uint16_t RECV_FW_UUID                      = 0x8020;
static const uint16_t OTA_BAR_UUID                      = 0x8021;
static const uint16_t COMMAND_UUID                      = 0x8022;
static const uint16_t CUSTOMER_UUID                     = 0x8023;

static uint8_t receive_fw_val[BLE_OTA_MAX_CHAR_VAL_LEN] = {0};
static uint8_t receive_fw_val_ccc[2] = {0x00, 0x00};

static uint8_t ota_status_val[20] = {0};
static uint8_t ota_status_val_ccc[2] = {0x00, 0x00};

static uint8_t command_val[20] = {0};
static uint8_t command_val_ccc[2] = {0x00, 0x00};

static uint8_t custom_val[20] = {0};
static uint8_t custom_val_ccc[2] = {0x00, 0x00};

static uint16_t ota_handle_table[OTA_IDX_NB];

/* OTA Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t ota_gatt_db[OTA_IDX_NB] = {
    // Service Declaration
    [OTA_SVC_IDX]        =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(BLE_OTA_SERVICE_UUID), sizeof(BLE_OTA_SERVICE_UUID), (uint8_t *) &BLE_OTA_SERVICE_UUID
        }
    },

    /* Characteristic Declaration */
    [RECV_FW_CHAR_IDX]      =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_write_indicate
        }
    },

    /* Characteristic Value */
    [RECV_FW_CHAR_VAL_IDX]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &RECV_FW_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(receive_fw_val), sizeof(receive_fw_val), (uint8_t *)receive_fw_val
        }
    },

    //data notify characteristic Declaration
    [RECV_FW_CHAR_NTF_CFG]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(receive_fw_val_ccc), sizeof(receive_fw_val_ccc), (uint8_t *)receive_fw_val_ccc
        }
    },

    //data receive characteristic Declaration
    [OTA_STATUS_CHAR_IDX]            =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_read_indicate
        }
    },

    //data receive characteristic Value
    [OTA_STATUS_CHAR_VAL_IDX]               =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &OTA_BAR_UUID, ESP_GATT_PERM_READ,
            sizeof(ota_status_val), sizeof(ota_status_val), (uint8_t *)ota_status_val
        }
    },

    //data notify characteristic Declaration
    [OTA_STATUS_NTF_CFG]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(ota_status_val_ccc), sizeof(ota_status_val_ccc), (uint8_t *)ota_status_val_ccc
        }
    },

    //data receive characteristic Declaration
    [CMD_CHAR_IDX]            =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_write_indicate
        }
    },

    //data receive characteristic Value
    [CMD_CHAR_VAL_IDX]              =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &COMMAND_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(command_val), sizeof(command_val), (uint8_t *)command_val
        }
    },

    //data notify characteristic Declaration
    [CMD_CHAR_NTF_CFG]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(command_val_ccc), sizeof(command_val_ccc), (uint8_t *)command_val_ccc
        }
    },

    //data receive characteristic Declaration
    [CUS_CHAR_IDX]            =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_write_indicate
        }
    },

    //data receive characteristic Value
    [CUS_CHAR_VAL_IDX]              =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &CUSTOMER_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(custom_val), sizeof(custom_val), (uint8_t *)custom_val
        }
    },

    //data notify characteristic Declaration
    [CUS_CHAR_NTF_CFG]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(custom_val_ccc), sizeof(custom_val_ccc), (uint8_t *)custom_val_ccc
        }
    },

};

static const uint16_t DIS_SERVICE_UUID    = 0x180A;
static const uint16_t DIS_MODEL_CHAR_UUID = 0x2A24;
static const uint16_t DIS_SN_CHAR_UUID    = 0x2A25;
static const uint16_t DIS_FW_CHAR_UUID    = 0x2A26;

static uint8_t dis_model_value[] = "Espressif";
static uint8_t dis_sn_value[]    = "esp-ota";
static uint8_t dis_fw_value[]    = "1.0";

static uint16_t dis_handle_table[DIS_IDX_NB];

/* DIS Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t dis_gatt_db[DIS_IDX_NB] = {
    // Service Declaration
    [DIS_SVC_IDX]        =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(DIS_SERVICE_UUID), sizeof(DIS_SERVICE_UUID), (uint8_t *) &DIS_SERVICE_UUID
        }
    },

    /* Characteristic Declaration */
    [DIS_MODEL_CHAR_IDX]      =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_read
        }
    },

    /* Characteristic Value */
    [DIS_MODEL_CHAR_VAL_IDX]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &DIS_MODEL_CHAR_UUID, ESP_GATT_PERM_READ,
            sizeof(dis_model_value), sizeof(dis_model_value), (uint8_t *)dis_model_value
        }
    },

    /* Characteristic Declaration */
    [DIS_SN_CHAR_IDX]      =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_read
        }
    },

    /* Characteristic Value */
    [DIS_SN_CHAR_VAL_IDX]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &DIS_SN_CHAR_UUID, ESP_GATT_PERM_READ,
            sizeof(dis_sn_value), sizeof(dis_sn_value), (uint8_t *)dis_sn_value
        }
    },

    /* Characteristic Declaration */
    [DIS_FW_CHAR_IDX]      =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &character_declaration_uuid, ESP_GATT_PERM_READ,
            CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &char_prop_read
        }
    },

    /* Characteristic Value */
    [DIS_FW_CHAR_VAL_IDX]  =
    {   {ESP_GATT_AUTO_RSP}, {
            ESP_UUID_LEN_16, (uint8_t *) &DIS_FW_CHAR_UUID, ESP_GATT_PERM_READ,
            sizeof(dis_fw_value), sizeof(dis_fw_value), (uint8_t *)dis_fw_value
        }
    },
};

static uint16_t crc16_ccitt(const unsigned char *buf, int len)
{
    uint16_t crc16 = 0;
    int32_t i;

    while (len--) {
        crc16 ^= *buf++ << 8;

        for (i = 0; i < 8; i++) {
            if (crc16 & 0x8000) {
                crc16 = (crc16 << 1) ^ 0x1021;
            } else {
                crc16 = crc16 << 1;
            }
        }
    }

    return crc16;
}

void esp_ble_ota_set_fw_length(unsigned int length)
{
    ota_total_len = length;
}

unsigned int esp_ble_ota_get_fw_length(void)
{
    return ota_total_len;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_bd_addr_t bd_addr;

    ESP_LOGD(TAG, "GAP_EVT, event %d\n", event);

    switch (event) {
#ifdef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
        esp_ble_gap_config_ext_adv_data_raw(EXT_ADV_HANDLE,  sizeof(ext_ota_adv_data), &ext_ota_adv_data[0]);
        break;
    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_ext_adv_start(NUM_EXT_ADV_SET, &ext_adv[0]);
        break;
    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "Ext adv start, status = %d", param->ext_adv_data_set.status);
        break;
#else
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ota_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed\n");
        }
        break;
#endif
    case ESP_GAP_BLE_SEC_REQ_EVT:
        for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
            ESP_LOGI(TAG, "%x:", param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "remote BD_ADDR: %08x%04x", \
                 (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                 (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(TAG, "pair status = %s", param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(TAG, "fail reason = 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    // ESP_LOGI(TAG, "EVT %d, gatts if %d\n", event, gatts_if);

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            ota_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d\n", param->reg.app_id, param->reg.status);
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < OTA_PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == ota_profile_tab[idx].gatts_if) {
                if (ota_profile_tab[idx].gatts_cb) {
                    ota_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

static void gatts_dis_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_err_t ret;

    ESP_LOGD(TAG, "%s - event: %d", __func__, event);

    switch (event) {
    case ESP_GATTS_REG_EVT:
        ret = esp_ble_gatts_create_attr_tab(dis_gatt_db, gatts_if, DIS_IDX_NB, DIS_PROFILE_APP_IDX);
        if (ret) {
            ESP_LOGE(TAG, "%s - create attr table failed, error code = %x", __func__, ret);
        }
        break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "DIS ESP_GATTS_READ_EVT");
        break;
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "DIS ESP_GATTS_WRITE_EVT");
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG, "DIS ESP_GATTS_EXEC_WRITE_EVT");
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "DIS ESP_GATTS_MTU_EVT  mtu = %d", param->mtu.mtu);
        break;
    case ESP_GATTS_CONF_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "DIS SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        if (param->start.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "SERVICE START FAIL, status %d", param->start.status);
            break;
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "dis create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        } else if (param->add_attr_tab.num_handle != DIS_IDX_NB) {
            ESP_LOGE(TAG, "dis create attribute table abnormally, num_handle (%d) doesn't equal to DIS_IDX_NB(%d)", param->add_attr_tab.num_handle, DIS_IDX_NB);
        } else {
            ESP_LOGI(TAG, "dis create attribute table successfully, the number handle = %d\n", param->add_attr_tab.num_handle);
            memcpy(dis_handle_table, param->add_attr_tab.handles, sizeof(dis_handle_table));
            esp_ble_gatts_start_service(dis_handle_table[DIS_SVC_IDX]);
        }
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_OPEN_EVT:
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
        break;
    case ESP_GATTS_CLOSE_EVT:
        break;
    case ESP_GATTS_LISTEN_EVT:
        break;
    case ESP_GATTS_CONGEST_EVT:
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    default:
        break;
    }
}

static esp_ble_ota_char_t find_ota_char_and_desr_by_handle(uint16_t handle)
{
    esp_ble_ota_char_t ret = INVALID_CHAR;

    for (int i = 0; i < OTA_IDX_NB ; i++) {
        if (handle == ota_handle_table[i]) {
            switch (i) {
            case RECV_FW_CHAR_VAL_IDX:
                ret = RECV_FW_CHAR;
                break;
            case RECV_FW_CHAR_NTF_CFG:
                ret = RECV_FW_CHAR_CCC;
                break;
            case OTA_STATUS_CHAR_VAL_IDX:
                ret = OTA_STATUS_CHAR;
                break;
            case OTA_STATUS_NTF_CFG:
                ret = OTA_STATUS_CHAR_CCC;
                break;
            case CMD_CHAR_VAL_IDX:
                ret = CMD_CHAR;
                break;
            case CMD_CHAR_NTF_CFG:
                ret = CMD_CHAR_CCC;
                break;
            case CUS_CHAR_VAL_IDX:
                ret = CUS_CHAR;
                break;
            case CUS_CHAR_NTF_CFG:
                ret = CUS_CHAR_CCC;
                break;
            default:
                ret = INVALID_CHAR;
                break;
            }
        }
    }

    return ret;
}

esp_err_t esp_ble_ota_recv_fw_handler(uint8_t *buf, unsigned int length)
{
    if (ota_cb_fun_t.recv_fw_cb) {
        ota_cb_fun_t.recv_fw_cb(buf, length);
    }

    return ESP_OK;
}

void esp_ble_ota_send_ack_data(ble_ota_ack_type_t ack_type, uint16_t ack_status, uint16_t ack_param)
{
    uint8_t cmd_ack[20] = {0};
    uint16_t crc16 = 0;

    switch (ack_type) {
    case BLE_OTA_CMD_ACK:
        cmd_ack[0] = (BLE_OTA_ACK_CMD & 0xff);
        cmd_ack[1] = (BLE_OTA_ACK_CMD & 0xff00) >> 8;

        cmd_ack[2] = (ack_param & 0xff);
        cmd_ack[3] = (ack_param & 0xff00) >> 8;

        cmd_ack[4] = (ack_status & 0xff);
        cmd_ack[5] = (ack_status & 0xff00) >> 8;

        crc16 = crc16_ccitt(cmd_ack, 18);
        cmd_ack[18] = crc16 & 0xff;
        cmd_ack[19] = (crc16 & 0xff00) >> 8;

        esp_ble_ota_notification_data(CMD_CHAR, cmd_ack, 20);
        break;
    case BLE_OTA_FW_ACK:
        cmd_ack[0] = (ack_param & 0xff);
        cmd_ack[1] = (ack_param & 0xff00) >> 8;

        cmd_ack[2] = (ack_status & 0xff);
        cmd_ack[3] = (ack_status & 0xff00) >> 8;

        cmd_ack[4] = (cur_sector & 0xff);
        cmd_ack[5] = (cur_sector & 0xff00) >> 8;

        crc16 = crc16_ccitt(cmd_ack, 18);
        cmd_ack[18] = crc16 & 0xff;
        cmd_ack[19] = (crc16 & 0xff00) >> 8;

        esp_ble_ota_notification_data(RECV_FW_CHAR, cmd_ack, 20);
        break;
    default:
        break;
    }
}

void esp_ble_ota_process_recv_data(esp_ble_ota_char_t ota_char, uint8_t *val, uint16_t val_len)
{
    unsigned int recv_sector = 0;

    switch (ota_char) {
    case CMD_CHAR:
        // Start BLE OTA Process
        if ((val[0] == 0x01) && (val[1] == 0x00)) {
            if (start_ota) {
                esp_ble_ota_send_ack_data(BLE_OTA_CMD_ACK, BLE_OTA_REJECT, BLE_OTA_START_CMD);
            } else {
                start_ota = true;
                // Calculating Firmware Length
                ota_total_len = (val[2]) +
                                (val[3] * 256) +
                                (val[4] * 256 * 256) +
                                (val[5] * 256 * 256 * 256);
                esp_ble_ota_set_fw_length(ota_total_len);
                ESP_LOGI(TAG, "Recv ota start cmd, fw_length = %d", ota_total_len);
                // Malloc buffer to store receive Firmware
                fw_buf = (uint8_t *)malloc(BUF_LENGTH * sizeof(uint8_t));
                if (fw_buf == NULL) {
                    ESP_LOGE(TAG, "Malloc fail");
                    break;
                } else {
                    memset(fw_buf, 0x0, BUF_LENGTH);
                }

                esp_ble_ota_send_ack_data(BLE_OTA_CMD_ACK, BLE_OTA_CMD_SUCCESS, BLE_OTA_START_CMD);
            }
        }
        // Stop BLE OTA Process
        else if ((val[0] == 0x02) && (val[1] == 0x00)) {
            if (start_ota) {
                extern SemaphoreHandle_t notify_sem;
                xSemaphoreTake(notify_sem, portMAX_DELAY);

                start_ota = false;
                esp_ble_ota_set_fw_length(0);

                xSemaphoreGive(notify_sem);

                ESP_LOGI(TAG, "recv ota stop cmd");
                esp_ble_ota_send_ack_data(BLE_OTA_CMD_ACK, BLE_OTA_CMD_SUCCESS, BLE_OTA_STOP_CMD);

                if (fw_buf) {
                    free(fw_buf);
                    fw_buf = NULL;
                }
            } else {
                esp_ble_ota_send_ack_data(BLE_OTA_CMD_ACK, BLE_OTA_REJECT, BLE_OTA_STOP_CMD);
            }

        } else {
            ESP_LOGE(TAG, "Unknown Command [0x%02x%02x]", val[1], val[0]);
        }
        break;
    case RECV_FW_CHAR:
        if (start_ota) {
            // Calculating the received sector index
            recv_sector = (val[0] + (val[1] * 256));

            if (recv_sector != cur_sector) { // sector error
                if (recv_sector == 0xffff) { // last sector
                    ESP_LOGI(TAG, "Laster sector");
                } else {  // sector error
                    ESP_LOGE(TAG, "Sector index error, cur: %d, recv: %d", cur_sector, recv_sector);
                    esp_ble_ota_send_ack_data(BLE_OTA_FW_ACK, BLE_OTA_FW_IND_ERR, recv_sector);
                }
            }

            if (val[2] != cur_packet) { // packet seq error
                if (val[2] == 0xff) { // last packet
                    ESP_LOGI(TAG, "laster packet");
                    goto write_ota_data;
                } else { // packet seq error
                    ESP_LOGE(TAG, "Packet index error, cur: %d, recv: %d", cur_packet, val[2]);
                }
            }
write_ota_data:
            memcpy(fw_buf + fw_buf_offset, val + 3, val_len - 3);
            fw_buf_offset += val_len - 3;
            ESP_LOGI(TAG, "DEBUG: Sector:%d, total length:%d, length:%d", cur_sector, fw_buf_offset, val_len - 3);
            if (val[2] == 0xff) {
                cur_packet = 0;
                cur_sector++;
                ESP_LOGD(TAG, "DEBUG: recv %d sector", cur_sector);
                goto sector_end;
            } else {
                cur_packet++;
            }

            break;
sector_end:
            esp_ble_ota_recv_fw_handler(fw_buf, 4096);
            memset(fw_buf, 0x0, 4096);
            fw_buf_offset = 0;
            esp_ble_ota_send_ack_data(BLE_OTA_FW_ACK, BLE_OTA_FW_SUCCESS, recv_sector);
        } else {
            ESP_LOGE(TAG, "BLE OTA hasn't started yet");
        }
        break;
    case OTA_STATUS_CHAR:
        break;
    case CUS_CHAR:
        break;
    default:
        ESP_LOGW(TAG, "Invalid data was received, char[%d]", ota_char);
        break;
    }
}

static void gatts_ota_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_err_t ret;
    esp_ble_ota_char_t ota_char;

    ESP_LOGI(TAG, "%s - event: %d", __func__, event);

    switch (event) {
    case ESP_GATTS_REG_EVT:
        ret = esp_ble_gap_set_device_name(DEVICE_NAME);
        if (ret) {
            ESP_LOGE(TAG, "set device name failed, error code = %x", ret);
        }
#ifdef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
        ret = esp_ble_gap_ext_adv_set_params(EXT_ADV_HANDLE, &ext_ota_adv_params);
        if (ret) {
            ESP_LOGE(TAG, "set ext adv params failed, error code = %x", ret);
        }
#else
        ret = esp_ble_gap_config_adv_data_raw((uint8_t *)ota_adv_data, sizeof(ota_adv_data));
        if (ret) {
            ESP_LOGE(TAG, "set adv data failed, error code = %x", ret);
        }

        ret = esp_ble_gap_config_scan_rsp_data_raw((uint8_t *)ota_scan_rsp_data, sizeof(ota_scan_rsp_data));
        if (ret) {
            ESP_LOGE(TAG, "set scan rsp data failed, error code = %x", ret);
        }
#endif

        ret = esp_ble_gatts_create_attr_tab(ota_gatt_db, gatts_if, OTA_IDX_NB, OTA_PROFILE_APP_IDX);
        if (ret) {
            ESP_LOGE(TAG, "Create attr table failed, error code = %x", ret);
        }
        break;
    case ESP_GATTS_READ_EVT:
        ota_char = find_ota_char_and_desr_by_handle(param->read.handle);
        ESP_LOGI(TAG, "Read event - ota_char: %d", ota_char);
        break;
    case ESP_GATTS_WRITE_EVT:
        ota_char = find_ota_char_and_desr_by_handle(param->write.handle);
        ESP_LOGD(TAG, "Write event - ota_char: %d", ota_char);
        // Enable indication
        if ((param->write.len == 2) && (param->write.value[0] == 0x02) && (param->write.value[1] == 0x00)) {
            if (ota_char == OTA_STATUS_CHAR_CCC) {
                ota_notification.process_bar_ntf_enable = true;
            }
            if (ota_char == RECV_FW_CHAR_CCC) {
                ota_notification.recv_fw_ntf_enable = true;
            }
            if (ota_char == CMD_CHAR_CCC) {
                ota_notification.command_ntf_enable = true;
            }
            if (ota_char == CUS_CHAR_CCC) {
                ota_notification.customer_ntf_enable = true;
            }
        }
        // Disable indication
        else if ((param->write.len == 2) && (param->write.value[0] == 0x00) && (param->write.value[1] == 0x00)) {
            if (ota_char == OTA_STATUS_CHAR_CCC) {
                ota_notification.process_bar_ntf_enable = false;
            }
            if (ota_char == RECV_FW_CHAR_CCC) {
                ota_notification.recv_fw_ntf_enable = false;
            }
            if (ota_char == CMD_CHAR_CCC) {
                ota_notification.command_ntf_enable = false;
            }
            if (ota_char == CUS_CHAR_CCC) {
                ota_notification.customer_ntf_enable = false;
            }
        }

        if (param->write.is_prep == false) {
            esp_ble_ota_process_recv_data(ota_char, param->write.value, param->write.len);
        } else {
            if (temp_prep_write_buf == NULL) {
                temp_prep_write_buf = (uint8_t *)malloc(BLE_OTA_MAX_CHAR_VAL_LEN * sizeof(uint8_t));
                if (temp_prep_write_buf == NULL) {
                    ESP_LOGE(TAG, "Malloc buffer for prep write fail");
                    break;
                }

                memset(temp_prep_write_buf, 0x0, BLE_OTA_MAX_CHAR_VAL_LEN);
                temp_buf_len = 0;
            }

            memcpy(temp_prep_write_buf + temp_buf_len, param->write.value, param->write.len);
            temp_buf_len += param->write.len;
        }
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
            if (temp_prep_write_buf) {
                esp_ble_ota_process_recv_data(RECV_FW_CHAR, temp_prep_write_buf, temp_buf_len);
            }
        } else {
            if (temp_prep_write_buf) {
                free(temp_prep_write_buf);
                temp_prep_write_buf = NULL;
            }

            temp_buf_len = 0;
        }
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT - mtu = %d", param->mtu.mtu);
        ota_profile_tab[OTA_PROFILE_APP_IDX].mtu_size = param->mtu.mtu;
        break;
    case ESP_GATTS_CONF_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        if (param->start.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "SERVICE START FAIL, status %d", param->start.status);
            break;
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        ota_profile_tab[OTA_PROFILE_APP_IDX].conn_id = param->connect.conn_id;
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x06;    // max_int = 0x6*1.25ms = 7.5ms
        conn_params.min_int = 0x06;    // min_int = 0x6*1.25ms = 7.5ms
        conn_params.timeout = 400;     // timeout = 400*10ms = 4000ms
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    case ESP_GATTS_DISCONNECT_EVT:
#ifdef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
        esp_ble_gap_ext_adv_start(NUM_EXT_ADV_SET, &ext_adv[0]);
#else
        esp_ble_gap_start_advertising(&ota_adv_params);
#endif
        ota_profile_tab[OTA_PROFILE_APP_IDX].mtu_size = 23;
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        } else if (param->add_attr_tab.num_handle != OTA_IDX_NB) {
            ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) doesn't equal to OTA_IDX_NB(%d)", param->add_attr_tab.num_handle, OTA_IDX_NB);
        } else {
            ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d\n", param->add_attr_tab.num_handle);
            memcpy(ota_handle_table, param->add_attr_tab.handles, sizeof(ota_handle_table));
            esp_ble_gatts_start_service(ota_handle_table[OTA_SVC_IDX]);
        }
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_OPEN_EVT:
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
        break;
    case ESP_GATTS_CLOSE_EVT:
        break;
    case ESP_GATTS_LISTEN_EVT:
        break;
    case ESP_GATTS_CONGEST_EVT:
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    default:
        break;
    }
}

static esp_err_t esp_ble_ota_set_characteristic_value(esp_ble_ota_service_index_t index, uint8_t *value, uint8_t length)
{
    esp_err_t ret;

    ret = esp_ble_gatts_set_attr_value(ota_handle_table[index], length, value);
    if (ret) {
        ESP_LOGE(TAG, "%s set attribute value fail: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_ble_ota_set_value(esp_ble_ota_char_t ota_char, uint8_t *value, uint8_t length)
{
    esp_err_t ret;

    switch (ota_char) {
    case RECV_FW_CHAR:
        ret = esp_ble_ota_set_characteristic_value(RECV_FW_CHAR_VAL_IDX, value, length);
        break;
    case OTA_STATUS_CHAR:
        ret = esp_ble_ota_set_characteristic_value(OTA_STATUS_CHAR_VAL_IDX, value, length);
        break;
    case CMD_CHAR:
        ret = esp_ble_ota_set_characteristic_value(CMD_CHAR_VAL_IDX, value, length);
        break;
    case CUS_CHAR:
        ret = esp_ble_ota_set_characteristic_value(CUS_CHAR_VAL_IDX, value, length);
        break;
    default:
        ret = ESP_FAIL;
        break;
    }

    if (ret) {
        ESP_LOGE(TAG, "%s set characteristic value fail: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t esp_ble_ota_send_indication(esp_ble_ota_service_index_t index, uint8_t *value, uint8_t length, bool need_ack)
{
    esp_err_t ret;
    uint16_t offset = 0;

    if (length <= (ota_profile_tab[OTA_PROFILE_APP_IDX].mtu_size - 3)) {
        ret = esp_ble_gatts_send_indicate(ota_profile_tab[OTA_PROFILE_APP_IDX].gatts_if, ota_profile_tab[OTA_PROFILE_APP_IDX].conn_id, ota_handle_table[index], length, value, need_ack);
        if (ret) {
            ESP_LOGE(TAG, "%s send notification fail: %s\n", __func__, esp_err_to_name(ret));
            return ESP_FAIL;
        }
    } else {
        while ((length - offset) > (ota_profile_tab[OTA_PROFILE_APP_IDX].mtu_size - 3)) {
            ret = esp_ble_gatts_send_indicate(ota_profile_tab[OTA_PROFILE_APP_IDX].gatts_if, ota_profile_tab[OTA_PROFILE_APP_IDX].conn_id, ota_handle_table[index], (ota_profile_tab[OTA_PROFILE_APP_IDX].mtu_size - 3), value + offset, need_ack);
            if (ret) {
                ESP_LOGE(TAG, "%s send notification fail: %s\n", __func__, esp_err_to_name(ret));
                return ESP_FAIL;
            }
            offset += (ota_profile_tab[OTA_PROFILE_APP_IDX].mtu_size - 3);
        }

        if ((length - offset) > 0) {
            ret = esp_ble_gatts_send_indicate(ota_profile_tab[OTA_PROFILE_APP_IDX].gatts_if, ota_profile_tab[OTA_PROFILE_APP_IDX].conn_id, ota_handle_table[index], (length - offset), value + offset, need_ack);
            if (ret) {
                ESP_LOGE(TAG, "%s send notification fail: %s\n", __func__, esp_err_to_name(ret));
                return ESP_FAIL;
            }
        }
    }

    return ESP_OK;
}

esp_err_t esp_ble_ota_notification_data(esp_ble_ota_char_t ota_char, uint8_t *value, uint8_t length)
{
    esp_err_t ret = ESP_FAIL;

    switch (ota_char) {
    case RECV_FW_CHAR:
        if (ota_notification.recv_fw_ntf_enable) {
            ret = esp_ble_ota_send_indication(RECV_FW_CHAR_VAL_IDX, value, length, false);
        } else {
            ESP_LOGE(TAG, "notify isn't enable");
        }
        break;
    case OTA_STATUS_CHAR:
        if (ota_notification.process_bar_ntf_enable) {
            ret = esp_ble_ota_send_indication(OTA_STATUS_CHAR_VAL_IDX, value, length, false);
        } else {
            ESP_LOGE(TAG, "notify isn't enable");
        }
        break;
    case CMD_CHAR:
        if (ota_notification.command_ntf_enable) {
            ret = esp_ble_ota_send_indication(CMD_CHAR_VAL_IDX, value, length, false);
        } else {
            ESP_LOGE(TAG, "notify isn't enable");
        }
        break;
    case CUS_CHAR:
        if (ota_notification.customer_ntf_enable) {
            ret = esp_ble_ota_send_indication(CUS_CHAR_VAL_IDX, value, length, false);
        } else {
            ESP_LOGE(TAG, "notify isn't enable");
        }
        break;
    default:
        ret = ESP_FAIL;
        break;
    }

    if (ret) {
        ESP_LOGE(TAG, "%s notification fail: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_ble_ota_recv_fw_data_callback(esp_ble_ota_recv_fw_cb_t callback)
{
    ota_cb_fun_t.recv_fw_cb = callback;
    return ESP_OK;
}

esp_err_t esp_ble_ota_host_init(void)
{
    esp_err_t ret;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
#else
    ret = esp_bluedroid_init();
#endif
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "ble ota Version[%d.%d.%d]\n", BLE_OTA_VER_MAJOR, BLE_OTA_VER_MINOR, BLE_OTA_VER_PATCH);

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);

    esp_ble_gatts_app_register(OTA_PROFILE_APP_IDX);
    esp_ble_gatts_app_register(DIS_PROFILE_APP_IDX);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    return ESP_OK;
}
#endif
