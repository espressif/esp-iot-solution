/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_os.h"
#include "esp_gmp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "esp_gmp_os";

/* OS profile group ID (from SPEC.md) */
#define ESP_GMP_GRP_OS 0x00

/* OS commands */
#define ESP_GMP_OS_CMD_CAP_QUERY     0x01  /* Query device capabilities */
#define ESP_GMP_OS_CMD_INFO_QUERY    0x02  /* Query device info */
#define ESP_GMP_OS_CMD_RESET         0x10  /* Reset device */

#define ESP_GMP_OS_CAP_SCHEMA_MAJOR  0x02
#define ESP_GMP_OS_CAP_SCHEMA_MINOR  0x02
#define ESP_GMP_OS_CAP_ROLE_DEVICE   0x01
#define ESP_GMP_OS_CAP_TRANSPORT_BLE 0x02
#define ESP_GMP_OS_CAP_RSP_LEN       12u

/* Runtime capability registration (Phase 2) */
static uint8_t s_capabilities = 0;
static portMUX_TYPE s_cap_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized = false;

/* Forward declarations */
static bool os_packet_handler(const esp_gmp_rx_t *pkt);
static void handle_cap_query(const esp_gmp_rx_t *pkt);

esp_err_t esp_gmp_os_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Register packet handler with GMP core */
    esp_err_t err = esp_gmp_register_handler(ESP_GMP_GRP_OS, os_packet_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register handler: %s", esp_err_to_name(err));
        return err;
    }

    s_capabilities = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "OS profile initialized");
    return ESP_OK;
}

void esp_gmp_os_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    esp_gmp_unregister_handler(ESP_GMP_GRP_OS, os_packet_handler);

    portENTER_CRITICAL(&s_cap_lock);
    s_capabilities = 0;
    portEXIT_CRITICAL(&s_cap_lock);

    s_initialized = false;
    ESP_LOGI(TAG, "OS profile deinitialized");
}

void esp_gmp_os_register_capability(uint8_t cap_bit)
{
    portENTER_CRITICAL(&s_cap_lock);
    s_capabilities |= cap_bit;
    portEXIT_CRITICAL(&s_cap_lock);

    ESP_LOGI(TAG, "Capability registered: 0x%02x (total: 0x%02x)", cap_bit, s_capabilities);
}

void esp_gmp_os_unregister_capability(uint8_t cap_bit)
{
    portENTER_CRITICAL(&s_cap_lock);
    s_capabilities &= ~cap_bit;
    portEXIT_CRITICAL(&s_cap_lock);

    ESP_LOGI(TAG, "Capability unregistered: 0x%02x (total: 0x%02x)", cap_bit, s_capabilities);
}

uint8_t esp_gmp_os_get_capabilities(void)
{
    uint8_t caps;
    portENTER_CRITICAL(&s_cap_lock);
    caps = s_capabilities;
    portEXIT_CRITICAL(&s_cap_lock);
    return caps;
}

/* ========== Packet Handler ========== */

static bool os_packet_handler(const esp_gmp_rx_t *pkt)
{
    if (!pkt || pkt->group_id != ESP_GMP_GRP_OS) {
        return false;
    }

    /* Responses are for host-side waiters (co-registered GRP_OS handlers). */
    if (pkt->op == ESP_GMP_OP_WRITE_RSP || pkt->op == ESP_GMP_OP_READ_RSP) {
        return false;
    }

    /* CAP_QUERY uses READ_REQ/READ_RSP per SPEC; accept WRITE_REQ for host compat */
    if (pkt->op != ESP_GMP_OP_READ_REQ && pkt->op != ESP_GMP_OP_WRITE_REQ) {
        return false;
    }

    switch (pkt->command_id) {
    case ESP_GMP_OS_CMD_CAP_QUERY:
        handle_cap_query(pkt);
        /* Replies via esp_gmp_reply; do not take frame_buf ownership. */
        return false;

    case ESP_GMP_OS_CMD_INFO_QUERY:
        /* TODO: Implement device info query */
        esp_gmp_reply(pkt, ESP_GMP_STATUS_NOT_SUPPORTED, NULL, 0);
        return false;

    case ESP_GMP_OS_CMD_RESET:
        /* TODO: Implement device reset */
        esp_gmp_reply(pkt, ESP_GMP_STATUS_NOT_SUPPORTED, NULL, 0);
        return false;

    default:
        /* Unknown command */
        esp_gmp_reply(pkt, ESP_GMP_STATUS_UNKNOWN_COMMAND, NULL, 0);
        return false;
    }
}

static void wr_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void wr_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/**
 * SPEC §11 12-byte CAP payload (matches BLE OTA example / host query_caps).
 */
static void handle_cap_query(const esp_gmp_rx_t *pkt)
{
    uint8_t caps = esp_gmp_os_get_capabilities();
    size_t max_payload = esp_gmp_max_payload_effective(pkt->link);
    if (max_payload > 0xFFFFFFFFu) {
        max_payload = 0xFFFFFFFFu;
    }

    uint8_t cap[ESP_GMP_OS_CAP_RSP_LEN];
    memset(cap, 0, sizeof(cap));
    cap[0] = ESP_GMP_OS_CAP_SCHEMA_MAJOR;
    cap[1] = ESP_GMP_OS_CAP_SCHEMA_MINOR;
    cap[2] = ESP_GMP_OS_CAP_ROLE_DEVICE;
    cap[3] = ESP_GMP_OS_CAP_TRANSPORT_BLE;
    /* max_control_payload: informational; use effective GMP payload capped to u16 */
    uint16_t max_ctrl = max_payload > 0xFFFFu ? 0xFFFFu : (uint16_t)max_payload;
    wr_be16(&cap[4], max_ctrl);
    wr_be32(&cap[6], (uint32_t)max_payload);
    cap[10] = (caps & ESP_GMP_OS_CAP_OTA_SUPPORTED) ? 1 : 0;
    /* Reserved in SPEC; expose FT for hosts that look here */
    cap[11] = (caps & ESP_GMP_OS_CAP_FT_SUPPORTED) ? 1 : 0;

    esp_err_t err = esp_gmp_reply(pkt, ESP_GMP_STATUS_OK, cap, sizeof(cap));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send CAP_QUERY response: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "CAP_QUERY response sent: ota=%u ft=%u max_payload=%u",
                 cap[10], cap[11], (unsigned)max_payload);
    }
}
