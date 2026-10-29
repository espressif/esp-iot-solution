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

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque link handle: stable pointer per connection (application supplied). */
typedef void *esp_gmp_link_t;

typedef struct {
    /** Send one complete GMP frame through the bound transport. */
    esp_err_t (*send)(void *ctx, const uint8_t *data, size_t len);
    /** Current maximum GMP payload, excluding the 10-byte GMP header. */
    size_t (*max_payload)(void *ctx);
    /** Optional readiness check; NULL means the transport can accept a send attempt. */
    bool (*can_send)(void *ctx);
} esp_gmp_transport_t;

typedef struct {
    esp_gmp_link_t link;
    uint8_t ver;
    uint8_t op;
    uint8_t group_id;
    uint16_t sequence;
    uint8_t command_id;
    uint8_t flags;
    uint8_t status;
    const uint8_t *payload;
    size_t payload_len;
    /** Full GMP frame buffer; on_packet may take ownership by returning true. */
    uint8_t *frame_buf;
    size_t frame_len;
} esp_gmp_rx_t;

typedef struct {
    uint8_t ver;
    uint8_t op;
    uint8_t group_id;
    uint16_t sequence;
    uint8_t command_id;
    uint8_t flags;
    uint8_t status;
} esp_gmp_tx_params_t;

/** Return true to take ownership of rx->frame_buf. */
typedef bool (*esp_gmp_on_packet_fn)(void *user_ctx, const esp_gmp_rx_t *pkt);

/** Base for esp_gmp-specific esp_err_t values. */
#define ESP_ERR_ESP_GMP_BASE 0x7600

/**
 * TX queue for this link is full; the frame was not queued.
 *
 * This is transient backpressure: the link is alive and the caller may retry
 * after in-flight frames complete. Distinct from ESP_ERR_INVALID_STATE, which
 * means the link is unregistered or closing and retrying will never succeed.
 */
#define ESP_ERR_ESP_GMP_TX_QUEUE_FULL (ESP_ERR_ESP_GMP_BASE + 0x01)

#define ESP_GMP_VER_SHIFT 4
#define ESP_GMP_VER       0x01

#define ESP_GMP_OP_READ_REQ   0x00
#define ESP_GMP_OP_READ_RSP   0x01
#define ESP_GMP_OP_WRITE_REQ  0x02
#define ESP_GMP_OP_WRITE_RSP  0x03

#define ESP_GMP_GRP_OS            0x00
#define ESP_GMP_GRP_OTA           0x01
#define ESP_GMP_GRP_FILE_TRANSFER 0x08

#define ESP_GMP_OS_CAP_QUERY       0x01
#define ESP_GMP_OTA_UPLOAD_DATA    0x01
#define ESP_GMP_OTA_UPLOAD_CONTROL 0x02
#define ESP_GMP_OTA_UPLOAD_QUERY   0x03

#define ESP_GMP_FLAG_CRC16_TAIL 0x01
#define ESP_GMP_FLAG_ABORT      0x02
#define ESP_GMP_FLAG_FRAGMENTED 0x04
#define ESP_GMP_FLAG_RFU_MASK   0xF8

typedef enum {
    ESP_GMP_STATUS_OK = 0x00,
    ESP_GMP_STATUS_UNKNOWN_COMMAND = 0x01,
    ESP_GMP_STATUS_BAD_STATE = 0x02,
    ESP_GMP_STATUS_BAD_LENGTH = 0x03,
    ESP_GMP_STATUS_CRC_ERROR = 0x05,
    ESP_GMP_STATUS_BUSY = 0x06,
    ESP_GMP_STATUS_NOT_SUPPORTED = 0x07,
    ESP_GMP_STATUS_NO_SESSION = 0x08,
    ESP_GMP_STATUS_SESSION_IN_USE = 0x09,
    ESP_GMP_STATUS_INTERNAL = 0xFF,
} esp_gmp_status_t;

#ifdef __cplusplus
}
#endif
