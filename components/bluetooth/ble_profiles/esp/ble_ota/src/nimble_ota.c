/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
*/

#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_uuid.h"
#include "ble_ota.h"
#include "freertos/semphr.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_nimble_hci.h"

/*
 * This is a workaround for the missing os_mbuf_len function in NimBLE.
 * It is not present in NimBLE 1.3, but is present in NimBLE 1.4.
 * This function is used to get the length of an os_mbuf.
 */
uint16_t
os_mbuf_len(const struct os_mbuf *om)
{
    uint16_t len;

    len = 0;
    while (om != NULL) {
        len += om->om_len;
        om = SLIST_NEXT(om, om_next);
    }

    return len;
}
#endif

#ifdef CONFIG_PRE_ENC_OTA
#define SECTOR_END_ID                       2
#define ENC_HEADER                          512
esp_decrypt_handle_t decrypt_handle_cmp;
#endif

#define BUF_LENGTH                          4098
#define OTA_IDX_NB                          4
#define CMD_ACK_LENGTH                      20

#define character_declaration_uuid          BLE_ATT_UUID_CHARACTERISTIC
#define character_client_config_uuid        BLE_ATT_UUID_CHARACTERISTIC

#define GATT_SVR_SVC_ALERT_UUID             0x1811
#define BLE_OTA_SERVICE_UUID                0x8018

#define RECV_FW_UUID                        0x8020
#define OTA_BAR_UUID                        0x8021
#define COMMAND_UUID                        0x8022
#define CUSTOMER_UUID                       0x8023

#if CONFIG_EXAMPLE_EXTENDED_ADV
static uint8_t ext_adv_pattern[] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0xab, 0xcd,
    0x03, 0x03, 0x18, 0x11,
    0x0f, 0X09, 'n', 'i', 'm', 'b', 'l', 'e', '-', 'o', 't', 'a', '-', 'e', 'x', 't',
};
#endif

static bool counter = false;
static const char *TAG = "NimBLE_BLE_OTA";

static bool start_ota = false;
static uint32_t cur_sector = 0;
static uint32_t cur_packet = 0;
static uint8_t *fw_buf = NULL;
static uint32_t fw_buf_offset = 0;

static uint32_t ota_total_len = 0;
static uint32_t ota_block_size = BUF_LENGTH;

esp_ble_ota_callback_funs_t ota_cb_fun_t = {
    .recv_fw_cb = NULL
};

#ifndef CONFIG_OTA_WITH_PROTOCOMM
esp_ble_ota_notification_check_t ota_notification = {
    .recv_fw_ntf_enable = false,
    .process_bar_ntf_enable = false,
    .command_ntf_enable = false,
    .customer_ntf_enable = false,
};

static uint8_t own_addr_type;

static uint16_t connection_handle;
static uint16_t attribute_handle;

static uint16_t ota_handle_table[OTA_IDX_NB];

static uint16_t receive_fw_val;
static uint16_t ota_status_val;
static uint16_t command_val;
static uint16_t custom_val;

static int
esp_ble_ota_gap_event(struct ble_gap_event *event, void *arg);
static esp_ble_ota_char_t
find_ota_char_and_desr_by_handle(uint16_t handle);
static int
esp_ble_ota_notification_data(uint16_t conn_handle, uint16_t attr_handle, uint8_t cmd_ack[],
                              esp_ble_ota_char_t ota_char);
#endif

/*----------------------------------------------------
 * Common api's
 *----------------------------------------------------*/

void ble_store_config_init(void);
static uint16_t
crc16_ccitt(const unsigned char *buf, int len);
static esp_err_t
esp_ble_ota_recv_fw_handler(uint8_t *buf, uint32_t length);

static void
esp_ble_ota_write_chr(struct os_mbuf *om)
{
#ifndef CONFIG_OTA_WITH_PROTOCOMM
    esp_ble_ota_char_t ota_char = find_ota_char_and_desr_by_handle(attribute_handle);
#endif

#ifdef CONFIG_PRE_ENC_OTA
    esp_err_t err;
    pre_enc_decrypt_arg_t pargs = {};

    pargs.data_in_len = os_mbuf_len(om) - 3;

    pargs.data_in = (const char *)malloc(pargs.data_in_len);
    err = os_mbuf_copydata(om, 3, pargs.data_in_len, pargs.data_in);

    if (om->om_data[2] == 0xff) {
        pargs.data_in_len -= SECTOR_END_ID;
    }

    err = esp_encrypted_img_decrypt_data(decrypt_handle_cmp, &pargs);
    if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED) {
        return;
    }
#endif

    uint8_t cmd_ack[CMD_ACK_LENGTH] = {0x03, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00
                                      };
    uint16_t crc16;

    if ((om->om_data[0] + (om->om_data[1] * 256)) != cur_sector) {
        // sector error
        if ((om->om_data[0] == 0xff) && (om->om_data[1] == 0xff)) {
            // last sector
            ESP_LOGD(TAG, "Last sector");
        } else {
            // sector error
            ESP_LOGE(TAG, "%s - sector index error, cur: %" PRIu32 ", recv: %d", __func__,
                     cur_sector, (om->om_data[0] + (om->om_data[1] * 256)));
            cmd_ack[0] = om->om_data[0];
            cmd_ack[1] = om->om_data[1];
            cmd_ack[2] = 0x02; //sector index error
            cmd_ack[3] = 0x00;
            cmd_ack[4] = cur_sector & 0xff;
            cmd_ack[5] = (cur_sector & 0xff00) >> 8;
            crc16 = crc16_ccitt(cmd_ack, 18);
            cmd_ack[18] = crc16 & 0xff;
            cmd_ack[19] = (crc16 & 0xff00) >> 8;
#ifndef CONFIG_OTA_WITH_PROTOCOMM
            esp_ble_ota_notification_data(connection_handle, attribute_handle, cmd_ack, ota_char);
#endif
        }
    }

    if (om->om_data[2] != cur_packet) { // packet seq error
        if (om->om_data[2] == 0xff) { // last packet
            ESP_LOGD(TAG, "last packet");
            goto write_ota_data;
        } else { // packet seq error
            ESP_LOGE(TAG, "%s - packet index error, cur: %" PRIu32 ", recv: %d", __func__,
                     cur_packet, om->om_data[2]);
        }
    }

write_ota_data:
#ifdef CONFIG_PRE_ENC_OTA
    if (pargs.data_out_len > 0) {
        memcpy(fw_buf + fw_buf_offset, pargs.data_out, pargs.data_out_len);

        free(pargs.data_out);
        free(pargs.data_in);

        fw_buf_offset += pargs.data_out_len;
    }
    ESP_LOGD(TAG, "DEBUG: Sector:%" PRIu32 ", total length:%" PRIu32 ", length:%d", cur_sector,
             fw_buf_offset, pargs.data_out_len);
#else
    os_mbuf_copydata(om, 3, os_mbuf_len(om) - 3, fw_buf + fw_buf_offset);
    fw_buf_offset += os_mbuf_len(om) - 3;

    ESP_LOGD(TAG, "DEBUG: Sector:%" PRIu32 ", total length:%" PRIu32 ", length:%d", cur_sector,
             fw_buf_offset, os_mbuf_len(om) - 3);
#endif
    if (om->om_data[2] == 0xff) {
        cur_packet = 0;
        cur_sector++;
        ESP_LOGD(TAG, "DEBUG: recv %" PRIu32 " sector", cur_sector);
        goto sector_end;
    } else {
        ESP_LOGD(TAG, "DEBUG: wait next packet");
        cur_packet++;
    }
    return;

sector_end:
    if (fw_buf_offset < ota_block_size) {
        esp_ble_ota_recv_fw_handler(fw_buf, fw_buf_offset);
    } else {
        esp_ble_ota_recv_fw_handler(fw_buf, 4096);
    }

    fw_buf_offset = 0;
    memset(fw_buf, 0x0, ota_block_size);

    cmd_ack[0] = om->om_data[0];
    cmd_ack[1] = om->om_data[1];
    cmd_ack[2] = 0x00; //success
    cmd_ack[3] = 0x00;
    crc16 = crc16_ccitt(cmd_ack, 18);
    cmd_ack[18] = crc16 & 0xff;
    cmd_ack[19] = (crc16 & 0xff00) >> 8;
    counter = true;
#ifndef CONFIG_OTA_WITH_PROTOCOMM
    esp_ble_ota_notification_data(connection_handle, attribute_handle, cmd_ack, ota_char);
#endif
}

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

void esp_ble_ota_set_fw_length(uint32_t length)
{
    ota_total_len = length;
}

unsigned int esp_ble_ota_get_fw_length(void)
{
    return ota_total_len;
}

static esp_err_t
esp_ble_ota_recv_fw_handler(uint8_t *buf, uint32_t length)
{
    if (ota_cb_fun_t.recv_fw_cb) {
        ota_cb_fun_t.recv_fw_cb(buf, length);
    }

    return ESP_OK;
}

#ifndef CONFIG_PRE_ENC_OTA
esp_err_t esp_ble_ota_recv_fw_data_callback(esp_ble_ota_recv_fw_cb_t callback)
{
    ota_cb_fun_t.recv_fw_cb = callback;
    return ESP_OK;
}
#else
esp_err_t esp_ble_ota_recv_fw_data_callback(esp_ble_ota_recv_fw_cb_t callback,
                                            esp_decrypt_handle_t esp_decrypt_handle)
{
    decrypt_handle_cmp = esp_decrypt_handle;
    ota_cb_fun_t.recv_fw_cb = callback;
    return ESP_OK;
}
#endif

/*----------------------------------------------------
 * Protocomm specific api's
 *----------------------------------------------------*/

#ifdef CONFIG_OTA_WITH_PROTOCOMM
void
esp_ble_ota_set_sizes(size_t file_size, size_t block_size)
{
    ota_total_len = file_size;
    ota_block_size = block_size;
}

void
esp_ble_ota_finish(void)
{
    ESP_LOGI(TAG, "Received OTA end command");
    start_ota = false;
    ota_total_len = 0;
    ota_block_size = 0;
    free(fw_buf);
    fw_buf = NULL;
}

void
esp_ble_ota_write(uint8_t *file, size_t length)
{
    struct os_mbuf *om = ble_hs_mbuf_from_flat(file, length);
    esp_ble_ota_write_chr(om);
}
#else

/*----------------------------------------------------
 * OTA without protocomm api's
 *----------------------------------------------------*/

static void
ble_ota_start_write_chr(struct os_mbuf *om)
{
    uint8_t cmd_ack[CMD_ACK_LENGTH] = {0x03, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00
                                      };
    uint16_t crc16;

    esp_ble_ota_char_t ota_char = find_ota_char_and_desr_by_handle(attribute_handle);
    if ((om->om_data[0] == 0x01) && (om->om_data[1] == 0x00)) {
        start_ota = true;

        ota_total_len = (om->om_data[2]) + (om->om_data[3] * 256) +
                        (om->om_data[4] * 256 * 256) + (om->om_data[5] * 256 * 256 * 256);

#ifdef CONFIG_PRE_ENC_OTA
        ota_total_len -= ENC_HEADER;
#endif
        ESP_LOGI(TAG, "recv ota start cmd, fw_length = %" PRIu32 "", ota_total_len);

        fw_buf = (uint8_t *)malloc(ota_block_size * sizeof(uint8_t));
        if (fw_buf == NULL) {
            ESP_LOGE(TAG, "%s -  malloc fail", __func__);
        } else {
            memset(fw_buf, 0x0, ota_block_size);
        }
        cmd_ack[2] = 0x01;
        cmd_ack[3] = 0x00;
        crc16 = crc16_ccitt(cmd_ack, 18);
        cmd_ack[18] = crc16 & 0xff;
        cmd_ack[19] = (crc16 & 0xff00) >> 8;
        esp_ble_ota_notification_data(connection_handle, attribute_handle, cmd_ack, ota_char);
    } else if ((om->om_data[0] == 0x02) && (om->om_data[1] == 0x00)) {
        printf("\nCMD_CHAR -> 0 : %d, 1 : %d", om->om_data[0],
               om->om_data[1]);
#ifdef CONFIG_PRE_ENC_OTA
        if (esp_encrypted_img_decrypt_end(decrypt_handle_cmp) != ESP_OK) {
            ESP_LOGI(TAG, "Decryption end failed");
        }
#endif
        extern SemaphoreHandle_t notify_sem;
        xSemaphoreTake(notify_sem, portMAX_DELAY);

        start_ota = false;
        ota_total_len = 0;

        xSemaphoreGive(notify_sem);

        ESP_LOGD(TAG, "recv ota stop cmd");
        cmd_ack[2] = 0x02;
        cmd_ack[3] = 0x00;
        crc16 = crc16_ccitt(cmd_ack, 18);
        cmd_ack[18] = crc16 & 0xff;
        cmd_ack[19] = (crc16 & 0xff00) >> 8;
        esp_ble_ota_notification_data(connection_handle, attribute_handle, cmd_ack, ota_char);
        free(fw_buf);
        fw_buf = NULL;
    }
}

static void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d\n",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with "
                 "def_handle=%d val_handle=%d\n",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d\n",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

static int
ble_ota_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt,
                     void *arg)
{
    esp_ble_ota_char_t ota_char;

    attribute_handle = attr_handle;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ota_char = find_ota_char_and_desr_by_handle(attr_handle);
        ESP_LOGI(TAG, "client read, ota_char: %d", ota_char);
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:

        ota_char = find_ota_char_and_desr_by_handle(attr_handle);
        ESP_LOGD(TAG, "client write; len = %d", os_mbuf_len(ctxt->om));

        if (ota_char == RECV_FW_CHAR) {
            if (start_ota) {
                esp_ble_ota_write_chr(ctxt->om);

            } else {
                ESP_LOGE(TAG, "%s -  don't receive the start cmd", __func__);
            }
        } else if (ota_char == CMD_CHAR) {
            ble_ota_start_write_chr(ctxt->om);
        }
        break;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
    return 0;
}

static const struct ble_gatt_svc_def ota_gatt_db[] = {
    {
        /* OTA Service Declaration */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_OTA_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
                /* Receive Firmware Characteristic */
                .uuid = BLE_UUID16_DECLARE(RECV_FW_UUID),
                .access_cb = ble_ota_gatt_handler,
                .val_handle = &receive_fw_val,
                .flags = BLE_GATT_CHR_F_INDICATE | BLE_GATT_CHR_F_WRITE,
            }, {
                /* OTA Characteristic */
                .uuid = BLE_UUID16_DECLARE(OTA_BAR_UUID),
                .access_cb = ble_ota_gatt_handler,
                .val_handle = &ota_status_val,
                .flags = BLE_GATT_CHR_F_INDICATE | BLE_GATT_CHR_F_READ,
            }, {
                /* Command Characteristic */
                .uuid = BLE_UUID16_DECLARE(COMMAND_UUID),
                .access_cb = ble_ota_gatt_handler,
                .val_handle = &command_val,
                .flags = BLE_GATT_CHR_F_INDICATE | BLE_GATT_CHR_F_WRITE,
            }, {
                /* Customer characteristic */
                .uuid = BLE_UUID16_DECLARE(CUSTOMER_UUID),
                .access_cb = ble_ota_gatt_handler,
                .val_handle = &custom_val,
                .flags = BLE_GATT_CHR_F_INDICATE | BLE_GATT_CHR_F_WRITE,
            }, {
                0, /* No more characteristics in this service */
            }
        },
    },
    {
        0, /* No more services */
    },
};

static esp_ble_ota_char_t
find_ota_char_and_desr_by_handle(uint16_t handle)
{
    esp_ble_ota_char_t ret = INVALID_CHAR;

    for (int i = 0; i < OTA_IDX_NB ; i++) {
        if (handle == ota_handle_table[i]) {
            switch (i) {
            case RECV_FW_CHAR_VAL_IDX:
                ret = RECV_FW_CHAR;
                break;
            case OTA_STATUS_CHAR_VAL_IDX:
                ret = OTA_STATUS_CHAR;
                break;
            case CMD_CHAR_VAL_IDX:
                ret = CMD_CHAR;
                break;
            case CUS_CHAR_VAL_IDX:
                ret = CUS_CHAR;
                break;
            default:
                ret = INVALID_CHAR;
                break;
            }
        }
    }
    return ret;
}

static int
esp_ble_ota_notification_data(uint16_t conn_handle, uint16_t attr_handle, uint8_t cmd_ack[],
                              esp_ble_ota_char_t ota_char)
{
    struct os_mbuf *txom;
    bool notify_enable = false;
    int rc;
    txom = ble_hs_mbuf_from_flat(cmd_ack, CMD_ACK_LENGTH);

    switch (ota_char) {
    case RECV_FW_CHAR:
        if (ota_notification.recv_fw_ntf_enable) {
            notify_enable = true;
        }
        break;
    case OTA_STATUS_CHAR:
        if (ota_notification.process_bar_ntf_enable) {
            notify_enable = true;
        }
        break;
    case CMD_CHAR:
        if (ota_notification.command_ntf_enable) {
            notify_enable = true;
        }
        break;
    case CUS_CHAR:
        if (ota_notification.customer_ntf_enable) {
            notify_enable = true;
        }
        break;
    case INVALID_CHAR:
        break;
    }

    if (notify_enable) {
        rc = ble_gattc_notify_custom(conn_handle, attr_handle, txom);
        if (rc == 0) {
            ESP_LOGD(TAG, "Notification sent, attr_handle = %d", attr_handle);
        } else {
            ESP_LOGE(TAG, "Error in sending notification, rc = %d", rc);
        }
        return rc;
    }

    /* If notifications are disabled return ESP_FAIL */
    ESP_LOGI(TAG, "Notify is disabled");
    return ESP_FAIL;
}

static void
esp_ble_ota_fill_handle_table(void)
{
    ota_handle_table[RECV_FW_CHAR] = receive_fw_val;
    ota_handle_table[OTA_STATUS_CHAR] = ota_status_val;
    ota_handle_table[CMD_CHAR] = command_val;
    ota_handle_table[CUS_CHAR] = custom_val;
}

static void
print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x",
             u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

/**
 * Logs information about a connection to the console.
 */
static void
esp_ble_ota_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "handle=%d our_ota_addr_type=%d our_ota_addr=",
             desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    ESP_LOGI(TAG, " our_id_addr_type=%d our_id_addr=",
             desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    ESP_LOGI(TAG, " peer_ota_addr_type=%d peer_ota_addr=",
             desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    ESP_LOGI(TAG, " peer_id_addr_type=%d peer_id_addr=",
             desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    ESP_LOGI(TAG, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
             "encrypted=%d authenticated=%d bonded=%d\n",
             desc->conn_itvl, desc->conn_latency,
             desc->supervision_timeout,
             desc->sec_state.encrypted,
             desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
#if CONFIG_EXAMPLE_EXTENDED_ADV
static void
esp_ble_ota_ext_advertise(void)
{
    struct ble_gap_ext_adv_params params;
    struct os_mbuf *data;
    uint8_t instance = 0;
    int rc;

    /* First check if any instance is already active */
    if (ble_gap_adv_active()) {
        return;
    }

    /* use defaults for non-set params */
    memset(&params, 0, sizeof(params));

    /* enable connectable advertising */
    params.connectable = 1;
    params.scannable = 1;

    /* advertise using random addr */
    params.own_addr_type = BLE_OWN_ADDR_PUBLIC;

    params.primary_phy = BLE_HCI_LE_PHY_1M;
    params.secondary_phy = BLE_HCI_LE_PHY_1M;
    params.sid = 1;
    params.legacy_pdu = 1;

    params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MIN;

    /* configure instance 0 */
    rc = ble_gap_ext_adv_configure(instance, &params, NULL,
                                   esp_ble_ota_gap_event, NULL);
    assert(rc == 0);
    /* in this case only scan response is allowed */

    /* get mbuf for scan rsp data */
    data = os_msys_get_pkthdr(sizeof(ext_adv_pattern), 0);
    assert(data);

    /* fill mbuf with scan rsp data */
    rc = os_mbuf_append(data, ext_adv_pattern, sizeof(ext_adv_pattern));
    assert(rc == 0);

    rc = ble_gap_ext_adv_set_data(instance, data);
    assert(rc == 0);

    /* start advertising */
    rc = ble_gap_ext_adv_start(instance, 0, 0);
    assert(rc == 0);
}
#else
static void
esp_ble_ota_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, esp_ble_ota_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}
#endif

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * esp_ble_ota uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  esp_ble_ota.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
esp_ble_ota_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;
    esp_ble_ota_char_t ota_char;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGI(TAG, "connection %s; status=%d ",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            esp_ble_ota_print_conn_desc(&desc);
            connection_handle = event->connect.conn_handle;
        } else {
            /* Connection failed; resume advertising. */
#if CONFIG_EXAMPLE_EXTENDED_ADV
            esp_ble_ota_ext_advertise();
#else
            esp_ble_ota_advertise();
#endif
        }
        esp_ble_ota_fill_handle_table();
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);
        esp_ble_ota_print_conn_desc(&event->disconnect.conn);

        /* Connection terminated; resume advertising. */
#if CONFIG_EXAMPLE_EXTENDED_ADV
        esp_ble_ota_ext_advertise();
#else
        esp_ble_ota_advertise();
#endif
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        ESP_LOGI(TAG, "connection updated; status=%d ",
                 event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        esp_ble_ota_print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d",
                 event->adv_complete.reason);
#ifdef CONFIG_EXAMPLE_EXTENDED_ADV
        esp_ble_ota_ext_advertise();
#else
        esp_ble_ota_advertise();
#endif
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        ESP_LOGI(TAG, "encryption change event; status=%d ",
                 event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        esp_ble_ota_print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ota_char = find_ota_char_and_desr_by_handle(event->subscribe.attr_handle);
        ESP_LOGI(TAG, "client subscribe ble_gap_event, ota_char: %d", ota_char);

        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d "
                 "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.reason,
                 event->subscribe.prev_notify,
                 event->subscribe.cur_notify,
                 event->subscribe.prev_indicate,
                 event->subscribe.cur_indicate);

        switch (ota_char) {
        case RECV_FW_CHAR:
            ota_notification.recv_fw_ntf_enable = true;
            break;
        case OTA_STATUS_CHAR:
            ota_notification.process_bar_ntf_enable = true;
            break;
        case CMD_CHAR:
            ota_notification.command_ntf_enable = true;
            break;
        case CUS_CHAR:
            ota_notification.customer_ntf_enable = true;
            break;
        case INVALID_CHAR:
            break;
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started \n");
        return 0;
    }

    return 0;
}

int
ble_ota_gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(ota_gatt_db);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(ota_gatt_db);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static void
esp_ble_ota_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d\n", reason);
}

static void
esp_ble_ota_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    ESP_LOGI(TAG, "Device Address: ");
    print_addr(addr_val);
    /* Begin advertising. */
#if CONFIG_EXAMPLE_EXTENDED_ADV
    esp_ble_ota_ext_advertise();
#else
    esp_ble_ota_advertise();
#endif

}

void esp_ble_ota_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t
esp_ble_ota_host_init(void)
{
    int rc;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (esp_nimble_init() != 0) {
        ESP_LOGE(TAG, "nimble host init failed\n");
        return ESP_FAIL;
    }
#else
    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
    nimble_port_init();
#endif

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = esp_ble_ota_on_reset;
    ble_hs_cfg.sync_cb = esp_ble_ota_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    rc = ble_ota_gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-ble-ota");
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(esp_ble_ota_host_task);

    return 0;
}
#endif
