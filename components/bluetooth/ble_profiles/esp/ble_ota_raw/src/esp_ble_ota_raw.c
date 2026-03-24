/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file
 *  @brief BLE OTA profile implementation on top of esp_ble_conn_mgr.
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_ble_ota_raw.h"
#include "esp_ble_ota_svc.h"
#include "esp_dis.h"

static const char *TAG = "esp_ble_ota_raw";

static esp_ble_ota_raw_recv_fw_cb_t s_recv_fw_cb;
static bool s_dis_inited_by_ota_raw = false;
typedef struct {
    bool start_ota;
    uint32_t ota_total_len;
    uint32_t recv_total_len;
    uint16_t cur_sector;
    uint8_t cur_packet;
    uint8_t *fw_sector_buf;
    uint32_t fw_sector_offset;
} ble_ota_context_t;

static ble_ota_context_t s_ota_ctx = {
    .ota_total_len = UINT32_MAX,
};

#define BLE_OTA_CMD_START                     0x0001
#define BLE_OTA_CMD_STOP                      0x0002
#define BLE_OTA_CMD_ACK                       0x0003
#define BLE_OTA_CMD_PACKET_LEN                20
#define BLE_OTA_CMD_CRC_DATA_LEN              18
#define BLE_OTA_FW_ACK_LEN                    20
#define BLE_OTA_FW_HEADER_LEN                 3
#define BLE_OTA_SECTOR_SIZE                   4096

typedef enum {
    BLE_OTA_CMD_ACK_ACCEPT = 0x0000,
    BLE_OTA_CMD_ACK_REJECT = 0x0001,
} ble_ota_cmd_ack_status_t;

typedef enum {
    BLE_OTA_FW_ACK_SUCCESS   = 0x0000,
    BLE_OTA_FW_ACK_CRC_ERR   = 0x0001,
    BLE_OTA_FW_ACK_INDEX_ERR = 0x0002,
    BLE_OTA_FW_ACK_LEN_ERR   = 0x0003,
} ble_ota_fw_ack_status_t;

/* CRC16-CCITT (poly 0x1021, init 0x0000), compatible with BLE OTA host tools. */
static uint16_t crc16_ccitt(const uint8_t *buf, uint16_t len)
{
    uint16_t crc16 = 0;

    while (len--) {
        crc16 ^= (uint16_t)(*buf++) << 8;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc16 & 0x8000) {
                crc16 = (uint16_t)((crc16 << 1) ^ 0x1021);
            } else {
                crc16 = (uint16_t)(crc16 << 1);
            }
        }
    }

    return crc16;
}

static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void reset_ota_state(bool free_sector_buf)
{
    s_ota_ctx.start_ota = false;
    s_ota_ctx.ota_total_len = UINT32_MAX;
    s_ota_ctx.recv_total_len = 0;
    s_ota_ctx.cur_sector = 0;
    s_ota_ctx.cur_packet = 0;
    s_ota_ctx.fw_sector_offset = 0;

    if (free_sector_buf && s_ota_ctx.fw_sector_buf) {
        free(s_ota_ctx.fw_sector_buf);
        s_ota_ctx.fw_sector_buf = NULL;
    }
}

static void reset_sector_rx_state(void)
{
    s_ota_ctx.fw_sector_offset = 0;
    s_ota_ctx.cur_packet = 0;
    if (s_ota_ctx.fw_sector_buf) {
        memset(s_ota_ctx.fw_sector_buf, 0, BLE_OTA_SECTOR_SIZE);
    }
}

/* Notify command ACK through COMMAND characteristic (0x8022). */
static esp_err_t send_cmd_ack(uint16_t cmd_id, ble_ota_cmd_ack_status_t ack_status)
{
    uint8_t ack[BLE_OTA_CMD_PACKET_LEN] = {0};
    uint16_t crc16;

    put_u16_le(&ack[0], BLE_OTA_CMD_ACK);
    put_u16_le(&ack[2], cmd_id);
    put_u16_le(&ack[4], (uint16_t)ack_status);
    crc16 = crc16_ccitt(ack, BLE_OTA_CMD_CRC_DATA_LEN);
    put_u16_le(&ack[18], crc16);

    return esp_ble_ota_notify_command_raw(ack, sizeof(ack));
}

/* Notify firmware ACK through RECV_FW characteristic (0x8020). */
static esp_err_t send_fw_ack(uint16_t sector, ble_ota_fw_ack_status_t ack_status, uint16_t expected_sector)
{
    uint8_t ack[BLE_OTA_FW_ACK_LEN] = {0};
    uint16_t crc16;

    put_u16_le(&ack[0], sector);
    put_u16_le(&ack[2], (uint16_t)ack_status);
    put_u16_le(&ack[4], expected_sector);
    crc16 = crc16_ccitt(ack, BLE_OTA_CMD_CRC_DATA_LEN);
    put_u16_le(&ack[18], crc16);

    return esp_ble_ota_notify_recv_fw_raw(ack, sizeof(ack));
}

void esp_ble_ota_recv_fw_data(const uint8_t *data, uint16_t len)
{
    if (!data || len < BLE_OTA_FW_HEADER_LEN) {
        return;
    }

    if (!s_ota_ctx.start_ota || !s_ota_ctx.fw_sector_buf) {
        ESP_LOGW(TAG, "recv fw before start cmd");
        return;
    }

    /* Frame layout: [sector:2][packet:1][payload...], packet=0xFF means sector tail. */
    const uint16_t recv_sector = get_u16_le(&data[0]);
    const uint8_t recv_packet = data[2];
    const bool sector_end = (recv_packet == 0xff);
    const uint8_t *payload = data + BLE_OTA_FW_HEADER_LEN;
    uint16_t payload_len = (uint16_t)(len - BLE_OTA_FW_HEADER_LEN);
    uint16_t received_crc = 0;

    if (recv_sector != s_ota_ctx.cur_sector) {
        ESP_LOGW(TAG, "sector mismatch, expected=0x%04x recv=0x%04x", s_ota_ctx.cur_sector, recv_sector);
        (void)send_fw_ack(recv_sector, BLE_OTA_FW_ACK_INDEX_ERR, s_ota_ctx.cur_sector);
        return;
    }

    if (!sector_end && recv_packet != s_ota_ctx.cur_packet) {
        ESP_LOGW(TAG, "packet mismatch, expected=0x%04x recv=0x%04x", s_ota_ctx.cur_packet, recv_packet);
        (void)send_fw_ack(recv_sector, BLE_OTA_FW_ACK_LEN_ERR, s_ota_ctx.cur_sector);
        return;
    }

    if (sector_end) {
        if (payload_len < 2) {
            ESP_LOGW(TAG, "sector end packet too short");
            (void)send_fw_ack(recv_sector, BLE_OTA_FW_ACK_LEN_ERR, s_ota_ctx.cur_sector);
            return;
        }
        received_crc = get_u16_le(&payload[payload_len - 2]);
        payload_len = (uint16_t)(payload_len - 2);
    }

    if ((s_ota_ctx.fw_sector_offset + payload_len) > BLE_OTA_SECTOR_SIZE) {
        ESP_LOGW(TAG, "sector payload overflow: offset=%" PRIu32 " append=%" PRIu16,
                 s_ota_ctx.fw_sector_offset, payload_len);
        reset_sector_rx_state();
        (void)send_fw_ack(recv_sector, BLE_OTA_FW_ACK_LEN_ERR, s_ota_ctx.cur_sector);
        return;
    }

    memcpy(s_ota_ctx.fw_sector_buf + s_ota_ctx.fw_sector_offset, payload, payload_len);
    s_ota_ctx.fw_sector_offset += payload_len;

    if (!sector_end) {
        s_ota_ctx.cur_packet++;
        return;
    }

    const uint16_t calc_crc = crc16_ccitt(s_ota_ctx.fw_sector_buf, (uint16_t)s_ota_ctx.fw_sector_offset);
    if (calc_crc != received_crc) {
        ESP_LOGW(TAG, "sector crc mismatch: calc=0x%04x recv=0x%04x", calc_crc, received_crc);
        reset_sector_rx_state();
        (void)send_fw_ack(recv_sector, BLE_OTA_FW_ACK_CRC_ERR, s_ota_ctx.cur_sector);
        return;
    }

    if (s_recv_fw_cb && s_ota_ctx.fw_sector_offset > 0) {
        s_recv_fw_cb(s_ota_ctx.fw_sector_buf, s_ota_ctx.fw_sector_offset);
    }

    s_ota_ctx.recv_total_len += s_ota_ctx.fw_sector_offset;
    s_ota_ctx.cur_sector++;
    reset_sector_rx_state();

    if (s_ota_ctx.ota_total_len > 0 && s_ota_ctx.recv_total_len > s_ota_ctx.ota_total_len) {
        ESP_LOGW(TAG, "received len over total len: recv=%lu total=%lu",
                 (unsigned long)s_ota_ctx.recv_total_len, (unsigned long)s_ota_ctx.ota_total_len);
    }

    (void)send_fw_ack(recv_sector, BLE_OTA_FW_ACK_SUCCESS, s_ota_ctx.cur_sector);
}

static void handle_start_cmd(const uint8_t *data)
{
    if (s_ota_ctx.start_ota) {
        ESP_LOGW(TAG, "start cmd while ota running");
        (void)send_cmd_ack(BLE_OTA_CMD_START, BLE_OTA_CMD_ACK_REJECT);
        return;
    }

    /* START command payload[2..5]: total firmware length in bytes (little-endian). */
    s_ota_ctx.ota_total_len = get_u32_le(&data[2]);
    if (s_ota_ctx.ota_total_len == 0) {
        ESP_LOGW(TAG, "reject start cmd with zero firmware length");
        reset_ota_state(false);
        (void)send_cmd_ack(BLE_OTA_CMD_START, BLE_OTA_CMD_ACK_REJECT);
        return;
    }

    s_ota_ctx.fw_sector_buf = (uint8_t *)malloc(BLE_OTA_SECTOR_SIZE);
    if (!s_ota_ctx.fw_sector_buf) {
        ESP_LOGE(TAG, "alloc sector buffer failed");
        reset_ota_state(true);
        (void)send_cmd_ack(BLE_OTA_CMD_START, BLE_OTA_CMD_ACK_REJECT);
        return;
    }

    memset(s_ota_ctx.fw_sector_buf, 0, BLE_OTA_SECTOR_SIZE);
    s_ota_ctx.start_ota = true;
    s_ota_ctx.recv_total_len = 0;
    s_ota_ctx.cur_sector = 0;
    s_ota_ctx.cur_packet = 0;
    s_ota_ctx.fw_sector_offset = 0;

    ESP_LOGI(TAG, "ota start, fw_len=%lu", (unsigned long)s_ota_ctx.ota_total_len);
    (void)send_cmd_ack(BLE_OTA_CMD_START, BLE_OTA_CMD_ACK_ACCEPT);
}

uint32_t esp_ble_ota_raw_get_fw_length(void)
{
    return s_ota_ctx.ota_total_len;
}

static void handle_stop_cmd(void)
{
    if (!s_ota_ctx.start_ota) {
        ESP_LOGW(TAG, "stop cmd while ota not running");
        (void)send_cmd_ack(BLE_OTA_CMD_STOP, BLE_OTA_CMD_ACK_REJECT);
        return;
    }

    ESP_LOGI(TAG, "ota stop, recv_total=%lu", (unsigned long)s_ota_ctx.recv_total_len);
    reset_ota_state(true);
    (void)send_cmd_ack(BLE_OTA_CMD_STOP, BLE_OTA_CMD_ACK_ACCEPT);
}

void esp_ble_ota_recv_cmd_data(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2) {
        return;
    }

    const uint16_t cmd_id = get_u16_le(data);

    if (len != BLE_OTA_CMD_PACKET_LEN) {
        ESP_LOGW(TAG, "invalid cmd len: %u", len);
        (void)send_cmd_ack(cmd_id, BLE_OTA_CMD_ACK_REJECT);
        return;
    }

    /* Command frame is fixed 20 bytes, CRC16 placed at bytes 18..19. */
    const uint16_t recv_crc = get_u16_le(&data[18]);
    const uint16_t calc_crc = crc16_ccitt(data, BLE_OTA_CMD_CRC_DATA_LEN);
    if (recv_crc != calc_crc) {
        ESP_LOGW(TAG, "invalid cmd crc: calc=0x%04x recv=0x%04x", calc_crc, recv_crc);
        (void)send_cmd_ack(cmd_id, BLE_OTA_CMD_ACK_REJECT);
        return;
    }

    switch (cmd_id) {
    case BLE_OTA_CMD_START:
        handle_start_cmd(data);
        break;
    case BLE_OTA_CMD_STOP:
        handle_stop_cmd();
        break;
    default:
        ESP_LOGW(TAG, "unknown cmd: 0x%04x", cmd_id);
        (void)send_cmd_ack(cmd_id, BLE_OTA_CMD_ACK_REJECT);
        break;
    }
}

void esp_ble_ota_recv_customer_data(const uint8_t *data, uint16_t len)
{
    ESP_LOGD(TAG, "recv customer data len=%u", len);
    (void)data;
}

esp_err_t esp_ble_ota_raw_recv_fw_data_callback(esp_ble_ota_raw_recv_fw_cb_t callback)
{
    s_recv_fw_cb = callback;
    return ESP_OK;
}

esp_err_t esp_ble_ota_raw_init(void)
{
    esp_err_t ret;

    ret = esp_ble_ota_svc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_ota_svc_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_dis_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_dis_init failed: %s", esp_err_to_name(ret));
        esp_ble_ota_svc_deinit();
        return ret;
    }
    s_dis_inited_by_ota_raw = true;

    return ESP_OK;
}

esp_err_t esp_ble_ota_raw_deinit(void)
{
    esp_err_t ret_dis = ESP_OK;
    esp_err_t ret_ota_svc;

    reset_ota_state(true);

    if (s_dis_inited_by_ota_raw) {
        ret_dis = esp_ble_dis_deinit();
        if (ret_dis != ESP_OK) {
            ESP_LOGE(TAG, "esp_ble_dis_deinit failed: %s", esp_err_to_name(ret_dis));
        }
        s_dis_inited_by_ota_raw = false;
    }

    ret_ota_svc = esp_ble_ota_svc_deinit();
    if (ret_ota_svc != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_ota_svc_deinit failed: %s", esp_err_to_name(ret_ota_svc));
    }

    if (ret_dis != ESP_OK) {
        return ret_dis;
    }
    return ret_ota_svc;
}
