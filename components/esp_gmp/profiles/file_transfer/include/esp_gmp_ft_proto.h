/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_FT_CMD_TRANSFER_META      0x01
#define ESP_FT_CMD_DATA_BLOCK         0x02
#define ESP_FT_CMD_FINAL_CONFIRM      0x03
#define ESP_FT_CMD_ABORT              0x04

#define ESP_FT_WIRE_STATUS_OK         0x00
#define ESP_FT_WIRE_STATUS_REJECTED   0x01
#define ESP_FT_WIRE_STATUS_FAILED     0x01

#define ESP_FT_META_REQ_HDR_LEN       56u
#define ESP_FT_FILE_NAME_MAX_LEN      32u
#define ESP_FT_META_REQ_MAX_LEN       88u
#define ESP_FT_MIN_EFFECTIVE_PAYLOAD  ESP_FT_META_REQ_MAX_LEN
#define ESP_FT_META_RSP_LEN           12u
#define ESP_FT_FINAL_HDR_LEN          48u
#define ESP_FT_FINAL_MAX_LEN          80u
#define ESP_FT_ABORT_LEN              8u
#define ESP_FT_DATA_REQ_HDR_LEN       12u
#define ESP_FT_DATA_RSP_LEN           12u

typedef enum {
    ESP_FT_REASON_OK = 0x0000,
    ESP_FT_REASON_INVALID_MESSAGE = 0x0001,
    ESP_FT_REASON_INVALID_FILE_NAME = 0x0002,
    ESP_FT_REASON_FILE_TOO_LARGE = 0x0003,
    ESP_FT_REASON_NO_SPACE = 0x0004,
    ESP_FT_REASON_BUSY = 0x0005,
    ESP_FT_REASON_CAPABILITY_MISMATCH = 0x0006,
    ESP_FT_REASON_LINK_ERROR = 0x0007,
    ESP_FT_REASON_DATA_SEND_FAILED = 0x0008,
    ESP_FT_REASON_FILE_OPEN_FAILED = 0x0009,
    ESP_FT_REASON_FILE_READ_FAILED = 0x000a,
    ESP_FT_REASON_FILE_WRITE_FAILED = 0x000b,
    ESP_FT_REASON_TIMEOUT = 0x000c,
    ESP_FT_REASON_HASH_MISMATCH = 0x000d,
    ESP_FT_REASON_ABORTED = 0x000e,
    ESP_FT_REASON_INTERNAL_ERROR = 0x000f,
    ESP_FT_REASON_REJECTED_BY_APP = 0x0010,
} esp_ft_reason_code_t;

typedef struct {
    uint32_t transfer_id;
    uint64_t file_size;
    uint32_t block_size;
    uint32_t total_blocks;
    uint8_t sha256[32];
    char file_name[ESP_FT_FILE_NAME_MAX_LEN + 1];
} ft_meta_request_t;

typedef struct {
    uint32_t transfer_id;
    uint8_t status;
    uint16_t reason_code;
    uint32_t accepted_block_size;
} ft_meta_response_t;

typedef struct {
    uint32_t transfer_id;
    uint32_t block_index;
    const uint8_t *data;
    uint32_t data_len;
} ft_data_request_t;

typedef struct {
    uint32_t transfer_id;
    uint32_t block_index;
    uint8_t status;
    uint16_t reason_code;
} ft_data_response_t;

typedef struct {
    uint32_t transfer_id;
    uint8_t status;
    uint16_t reason_code;
    uint64_t received_size;
    uint8_t received_sha256[32];
    char saved_name[ESP_FT_FILE_NAME_MAX_LEN + 1];
} ft_final_confirm_t;

typedef struct {
    uint32_t transfer_id;
    uint16_t reason_code;
} ft_abort_t;

esp_err_t ft_protocol_encode_meta_request(const ft_meta_request_t *msg, uint8_t *buf,
                                          size_t buf_size, size_t *encoded_len);
esp_err_t ft_protocol_decode_meta_request(const uint8_t *buf, size_t len, ft_meta_request_t *msg);
esp_err_t ft_protocol_encode_meta_response(const ft_meta_response_t *msg, uint8_t *buf,
                                           size_t buf_size);
esp_err_t ft_protocol_decode_meta_response(const uint8_t *buf, size_t len, ft_meta_response_t *msg);
esp_err_t ft_protocol_decode_data_request(const uint8_t *buf, size_t len, ft_data_request_t *msg);
esp_err_t ft_protocol_encode_data_response(const ft_data_response_t *msg, uint8_t *buf,
                                           size_t buf_size);
esp_err_t ft_protocol_decode_data_response(const uint8_t *buf, size_t len, ft_data_response_t *msg);
esp_err_t ft_protocol_encode_final_confirm(const ft_final_confirm_t *msg, uint8_t *buf,
                                           size_t buf_size, size_t *encoded_len);
esp_err_t ft_protocol_decode_final_confirm(const uint8_t *buf, size_t len, ft_final_confirm_t *msg);
esp_err_t ft_protocol_encode_abort(const ft_abort_t *msg, uint8_t *buf, size_t buf_size);
esp_err_t ft_protocol_decode_abort(const uint8_t *buf, size_t len, ft_abort_t *msg);
void ft_protocol_encode_data_request_header(uint8_t buf[ESP_FT_DATA_REQ_HDR_LEN],
                                            uint32_t transfer_id, uint32_t block_index,
                                            uint32_t data_len);

#ifdef __cplusplus
}
#endif
