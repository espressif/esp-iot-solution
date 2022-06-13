/* OTA example, ble download compressed image

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "compressed_ota_download_common.h"

#if CONFIG_EXAMPLE_BLE_DOWNLOAD

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "bootloader_custom_ota.h"

static const char *TAG = "example_ble_d";
static xQueueHandle data_pointer_queue = NULL;

#ifdef CONFIG_EXAMPLE_USE_AES128
#include "mbedtls/aes.h"
#include "esp_efuse.h"
#define AES128_BLOCK_SZIE (16)
#define AES128_KEY_SZIE   (16)
#endif

#define BUFFSIZE                            1024
#define ALIGN_UP(num, align)                (((num) + ((align) - 1)) & ~((align) - 1))
#define OTA_URL_SIZE                        256
#define UPDATE_PARTITION_RESERVED_SPACE     (1024 * 8)

struct AMessage
{
    bool   need_rep;
    uint8_t * ucData;
    uint16_t ucData_len;
    uint16_t conn_id;
    esp_gatt_if_t gatt_if;
    uint32_t trans_id;
    uint16_t handle;   // write handle,
} xMessage;

enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,

    IDX_CHAR_C,
    IDX_CHAR_VAL_C,

    HRS_IDX_NB,
};

#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define SAMPLE_DEVICE_NAME          "ESP_GATTS_OTA"
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
*/
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define PREPARE_BUF_MAX_SIZE        1024
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)
#define BLE_ONCE_PACKETS_SIZE        512
static uint8_t adv_config_done       = 0;

uint16_t heart_rate_handle_table[HRS_IDX_NB];

typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;

static prepare_type_env_t prepare_write_env;

#if (CONFIG_BT_BLE_50_FEATURES_SUPPORTED)

#define HEART_PROFILE_NUM                         1
#define HEART_PROFILE_APP_IDX                     0
#define ESP_HEART_RATE_APP_ID                     0x55
#define HEART_RATE_SVC_INST_ID                    0
#define EXT_ADV_HANDLE                            0
#define NUM_EXT_ADV_SET                           1
#define EXT_ADV_DURATION                          0
#define EXT_ADV_MAX_EVENTS                        0

static uint8_t ext_adv_raw_data[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd,
        0x11, 0x09, 'E', 'S', 'P', '_', 'G', 'A', 'T', 'T', 'S', '_', 'O','T', 'A', '_', '5', '0'
};

static uint8_t legacy_adv_data[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb,
        0x11, 0x09, 'E', 'S', 'P', '_', 'G', 'A', 'T', 'T', 'S', '_', 'O','T', 'A', '_', '4', '2'
};

uint8_t addr_legacy[6] = {0xc0, 0xde, 0x52, 0x00, 0x00, 0x03};
static bool set_legacy_adv = false;

esp_ble_gap_ext_adv_params_t legacy_adv_params = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
    .interval_min = 0x45,
    .interval_max = 0x45,
    .channel_map = ADV_CHNL_ALL,
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_1M,
    .sid = 2,
    .scan_req_notif = false,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
};

esp_ble_gap_ext_adv_params_t ext_adv_params_2M = {
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
};

static esp_ble_gap_ext_adv_t ext_adv[2] = {
    [0] = {0, 0, 0},
    [1] = {1, 0, 0},
};

#else
static uint8_t raw_adv_data[] = {
        /* flags */
        0x02, 0x01, 0x06,
        /* tx power*/
        0x02, 0x0a, 0xeb,
        /* service uuid */
        0x03, 0x03, 0xFF, 0x00,
        /* device name */
        0x11, 0x09, 'E', 'S', 'P', '_', 'G', 'A', 'T', 'T', 'S', '_', 'O','T', 'A', '_', '4', '2'
};
static uint8_t raw_scan_rsp_data[] = {
        /* flags */
        0x02, 0x01, 0x06,
        /* tx power */
        0x02, 0x0a, 0xeb,
        /* service uuid */
        0x03, 0x03, 0xFF,0x00
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
#endif /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
					esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/* Service */
static const uint16_t GATTS_SERVICE_UUID_TEST      = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_TEST_A       = 0xFF01;
static const uint16_t GATTS_CHAR_UUID_TEST_B       = 0xFF02;
static const uint16_t GATTS_CHAR_UUID_TEST_C       = 0xFF03;

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_write          = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t heart_measurement_ccc[2]      = {0x00, 0x00};
static const uint8_t char_value[4]                 = {0x11, 0x22, 0x33, 0x44};

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
{
    // Service Declaration
    [IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t *)&GATTS_SERVICE_UUID_TEST}},

    /* Characteristic Declaration */
    [IDX_CHAR_A]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_A] =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_A, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_A]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

    /* Characteristic Declaration */
    [IDX_CHAR_B]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_B]  =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_B, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* Characteristic Declaration */
    [IDX_CHAR_C]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_C]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_C, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},
};

#ifdef CONFIG_EXAMPLE_USE_AES128
static esp_err_t get_aes128_key(uint8_t aes_key[16])
{
    if (aes_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#ifdef CONFIG_AES_KEY_STORE_IN_FLASH
    uint8_t origin_key[] = {0xEF, 0xBF, 0xBD, 0x36, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0x26, 0x6B, 0x61};
#elif CONFIG_AES_KEY_STORE_IN_EFUSE
    uint8_t origin_key[16] = {0};
    esp_efuse_read_block(EFUSE_BLK3, origin_key, 16 * 8, sizeof(uint8_t) * AES128_KEY_SZIE * 8);
#endif

    memcpy(aes_key, origin_key, AES128_KEY_SZIE);
    return ESP_OK;
}

static void disp_buf(const char *TAG, uint8_t *buf, uint32_t len)
{
    int i;
    assert(buf != NULL);
    for (i = 0; i < len; i++) {
        printf("0x%02x, ", buf[i]);
        if ((i + 1) % 32 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}
#endif

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
#if (CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
        case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
            ESP_LOGI(TAG,"ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT status %d",  param->ext_adv_set_params.status);
            if(!set_legacy_adv) {
                esp_ble_gap_config_ext_adv_data_raw(0,  sizeof(ext_adv_raw_data), &ext_adv_raw_data[0]);
            } else {
                esp_ble_gap_ext_adv_set_rand_addr(1, addr_legacy);
            }
            break;
        case ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT, status %d", param->ext_adv_set_rand_addr.status);
            esp_ble_gap_config_ext_adv_data_raw(1,  sizeof(legacy_adv_data), &legacy_adv_data[0]);
            break;
        case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG,"ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT status %d",  param->ext_adv_data_set.status);
            if(!set_legacy_adv) {
                esp_ble_gap_ext_adv_set_params(1, &legacy_adv_params);
                set_legacy_adv = true;
            } else {
                esp_ble_gap_ext_adv_start(2, &ext_adv[0]);
                set_legacy_adv = false;
            }
            break;
        case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT, status = %d", param->ext_adv_data_set.status);
            break;
        case ESP_GAP_BLE_READ_PHY_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_GAP_BLE_READ_PHY_COMPLETE_EVT, status = %d, tx_phy = %d, rx_phy = %d",
                param->read_phy.status, param->read_phy.tx_phy, param->read_phy.rx_phy);
            esp_log_buffer_hex("read_phy bda:", param->read_phy.bda, 6);
            break;
#else
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "advertising start failed");
            }else{
                ESP_LOGI(TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(TAG, "Stop adv successfully\n");
            }
            break;
#endif /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
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

void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(TAG, "prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
    esp_gatt_status_t status = ESP_GATT_OK;
    if (prepare_write_env->prepare_buf == NULL) {
        prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) {
            ESP_LOGE(TAG, "%s, Gatt_server prep no mem", __func__);
            status = ESP_GATT_NO_RESOURCES;
        }
    } else {
        if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }
    /*send response when param->write.need_rsp is true */
    if (param->write.need_rsp){
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp != NULL){
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK){
               ESP_LOGE(TAG, "Send response error");
            }
            free(gatt_rsp);
        }else{
            ESP_LOGE(TAG, "%s, malloc failed", __func__);
        }
    }
    if (status != ESP_GATT_OK){
        return;
    }
    memcpy(prepare_write_env->prepare_buf + param->write.offset,
           param->write.value,
           param->write.len);
    prepare_write_env->prepare_len += param->write.len;

}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && prepare_write_env->prepare_buf){
        esp_log_buffer_hex(TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    }else{
        ESP_LOGI(TAG,"ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:{
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
            if (set_dev_name_ret){
                ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }
#if (CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
             esp_ble_gap_ext_adv_set_params(EXT_ADV_HANDLE, &ext_adv_params_2M);
#else
            esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
            if (raw_adv_ret){
                ESP_LOGE(TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;
            esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
            if (raw_scan_ret){
                ESP_LOGE(TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
#endif /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
            break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_READ_EVT");
            break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep){
                // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
                //ESP_LOGI(TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
                uint8_t * spp_cmd_buff = NULL;
                struct AMessage pxMessage;
                spp_cmd_buff = (uint8_t *)malloc(BLE_ONCE_PACKETS_SIZE * sizeof(uint8_t));
                pxMessage.ucData_len = param->write.len;
                memcpy(spp_cmd_buff, param->write.value, param->write.len);
                pxMessage.ucData = spp_cmd_buff;
                pxMessage.need_rep = param->write.need_rsp;
                pxMessage.conn_id = param->write.conn_id;
                pxMessage.trans_id = param->write.trans_id;
                pxMessage.gatt_if = gatts_if;
                pxMessage.handle = param->write.handle;
                xQueueSend(data_pointer_queue,  (void *)&pxMessage, 10/portTICK_PERIOD_MS);
                //esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                if (heart_rate_handle_table[IDX_CHAR_CFG_A] == param->write.handle && param->write.len == 2){
                    uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                    if (descr_value == 0x0001){
                        ESP_LOGI(TAG, "notify enable");
                    }else if (descr_value == 0x0002){
                        ESP_LOGI(TAG, "indicate enable");
                    }
                    else if (descr_value == 0x0000){
                        ESP_LOGI(TAG, "notify/indicate disable ");
                    }else{
                        ESP_LOGE(TAG, "unknown descr value");
                        esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                    }

                }
            }else{
                /* handle prepare write */
                ESP_LOGE(TAG, "It is not supported to use the prepare write request method to transmit firmware, please use the write request method");
                example_prepare_write_event_env(gatts_if, &prepare_write_env, param);
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            // the length of gattc prepare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            example_exec_write_event_env(&prepare_write_env, param);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
#if (CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
            esp_ble_gap_read_phy(param->connect.remote_bda);
#endif
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
#if (CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
            esp_ble_gap_ext_adv_start(2, &ext_adv[0]);
#else
            esp_ble_gap_start_advertising(&adv_params);
#endif
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
                ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
            }
            else {
                ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(heart_rate_handle_table, param->add_attr_tab.handles, sizeof(heart_rate_handle_table));
                esp_ble_gatts_start_service(heart_rate_handle_table[IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if) {
                if (heart_rate_profile_tab[idx].gatts_cb) {
                    heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void ble_init(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(ESP_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(BLE_ONCE_PACKETS_SIZE);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}

esp_err_t example_storage_compressed_image(const esp_partition_t *update_partition)
{
    esp_err_t err;
    uint32_t wrote_size = 0;
    data_pointer_queue = xQueueCreate(10, sizeof( struct AMessage ));
#ifdef CONFIG_EXAMPLE_USE_AES128
    mbedtls_aes_context aes;
    uint8_t aes128_key[16] = {0};
    get_aes128_key(aes128_key);
    disp_buf(TAG, aes128_key, 16); // This log is only for testing, please remove this sentence when releasing the product
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, (const unsigned char *)aes128_key, 128);
    unsigned char encry_buf[AES128_BLOCK_SZIE] = {0};
    unsigned char decry_buf[AES128_BLOCK_SZIE] = {0};
    unsigned char *forward1 = NULL;
    unsigned char *forward2 = NULL;
    int decry_count = 0;
    int total_decry_count = 0;
    int actually_encry_size = 0;
    char *decryed_buf = calloc(1, BUFFSIZE);
    if (decryed_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for decryed data");
        goto err;
    }
#endif
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    struct AMessage recv_pxMessage;
    while (1) {
        if(xQueueReceive(data_pointer_queue,  (void *)&recv_pxMessage, portMAX_DELAY)) {
            if(recv_pxMessage.handle == heart_rate_handle_table[IDX_CHAR_VAL_A] && recv_pxMessage.ucData_len > 0) {
#ifdef CONFIG_EXAMPLE_USE_AES128
               /*if aes encryption is enabled, decrypt the ota_write_data buffer and rewrite the decrypted data to the buffer*/
               forward1 = (unsigned char *)recv_pxMessage.ucData;
               forward2 = (unsigned char *)decryed_buf;
               for (decry_count = 0; decry_count < recv_pxMessage.ucData_len; decry_count += AES128_BLOCK_SZIE) {
                  memcpy(encry_buf, forward1, AES128_BLOCK_SZIE);
                  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char *)encry_buf, (unsigned char *)decry_buf);
                  memcpy(forward2, decry_buf, AES128_BLOCK_SZIE);
                  forward1 += AES128_BLOCK_SZIE;
                  forward2 += AES128_BLOCK_SZIE;
               }

               total_decry_count += recv_pxMessage.ucData_len;
               memcpy(recv_pxMessage.ucData, decryed_buf, recv_pxMessage.ucData_len);
#endif
               if (image_header_was_checked == false) {
                  bootloader_custom_ota_header_t custom_ota_header = {0};
                  if (recv_pxMessage.ucData_len > sizeof(bootloader_custom_ota_header_t)) {
                     memcpy(&custom_ota_header, recv_pxMessage.ucData, sizeof(bootloader_custom_ota_header_t));
                     // check whether it is a compressed app.bin(include diff compressed ota and compressed only ota).
                     if (!strcmp((char *)custom_ota_header.header.magic, BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC)) {
                           ESP_LOGW(TAG, "compressed ota detected");
                     }
#ifdef CONFIG_EXAMPLE_USE_AES128
                     actually_encry_size = sizeof(custom_ota_header) + custom_ota_header.header.length;
#if CONFIG_SECURE_BOOT_V2_ENABLED || CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT
                     actually_encry_size = ALIGN_UP(actually_encry_size, 4096) + 4096; // secure boot is 4096-bytes alignment, and the signature block is 4096bytes.
#endif
#endif
                     // Check compressed image bin size
                     if (custom_ota_header.header.length > update_partition->size - UPDATE_PARTITION_RESERVED_SPACE) {
                           ESP_LOGE(TAG, "Compressed image size is too large");
                           goto err;
                     }
                     image_header_was_checked = true;
                  } else {
                     ESP_LOGE(TAG, "received package is not fit len");
                     goto err;
                  }
               }
               err = esp_partition_write(update_partition, wrote_size, (const void *)recv_pxMessage.ucData, recv_pxMessage.ucData_len);
               if (err != ESP_OK) {
                  ESP_LOGE(TAG, "write ota data error is %s", esp_err_to_name(err));
                  goto err;
               }
               wrote_size += recv_pxMessage.ucData_len;
               ESP_LOGD(TAG, "Written image length %d", wrote_size);
            } else if(recv_pxMessage.handle == heart_rate_handle_table[IDX_CHAR_VAL_B] && recv_pxMessage.ucData_len > 0) {
               // check downloaded compressed image
#ifdef CONFIG_EXAMPLE_USE_AES128
                mbedtls_aes_free(&aes);
                free(decryed_buf);
#endif
                ESP_LOGD(TAG, "Total Written image length %d, ota end ....", wrote_size);
                ESP_LOGI(TAG, "Prepare to restart system!");
#if CONFIG_SECURE_BOOT_V2_ENABLED || CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT
#ifdef CONFIG_EXAMPLE_USE_AES128
                ESP_LOGI(TAG, "address: %0xu, signed: %0xu", update_partition->address, actually_encry_size - sizeof(ets_secure_boot_signature_t));
                esp_err_t verify_err = esp_secure_boot_verify_signature(update_partition->address, actually_encry_size - sizeof(ets_secure_boot_signature_t));
#else
                ESP_LOGI(TAG, "address: %0xu, signed: %0xu", update_partition->address, wrote_size - sizeof(ets_secure_boot_signature_t));
                esp_err_t verify_err = esp_secure_boot_verify_signature(update_partition->address, wrote_size - sizeof(ets_secure_boot_signature_t));
#endif
                if (verify_err != ESP_OK) {
                    ESP_LOGE(TAG, "verify signature err=0x%x", verify_err);
                    goto err;
                } else {
                    ESP_LOGW(TAG, "verify signature success");
                }
#endif
                vQueueDelete(data_pointer_queue);
                return ESP_OK;
            }
            free(recv_pxMessage.ucData);
            esp_ble_gatts_send_response(recv_pxMessage.gatt_if, recv_pxMessage.conn_id, recv_pxMessage.trans_id, ESP_GATT_OK, NULL);
        }
    }
err:
    vQueueDelete(data_pointer_queue);
#ifdef CONFIG_EXAMPLE_USE_AES128
    if (decryed_buf) {
        free(decryed_buf);
    }
#endif
    return ESP_FAIL;
}

#endif /*CONFIG_EXAMPLE_BLE_DOWNLOAD */