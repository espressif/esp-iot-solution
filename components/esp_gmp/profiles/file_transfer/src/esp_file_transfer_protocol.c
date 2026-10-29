/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_ft_proto.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

static uint16_t get_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t get_be64(const uint8_t *p)
{
    return ((uint64_t)get_be32(p) << 32) | get_be32(p + 4);
}

static void put_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void put_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void put_be64(uint8_t *p, uint64_t value)
{
    put_be32(p, (uint32_t)(value >> 32));
    put_be32(p + 4, (uint32_t)value);
}

static bool reason_valid(uint16_t reason)
{
    return reason <= ESP_FT_REASON_REJECTED_BY_APP;
}

static bool utf8_name_valid(const uint8_t *name, size_t len)
{
    if (!name || len == 0 || len > ESP_FT_FILE_NAME_MAX_LEN ||
            (len == 1 && name[0] == '.') ||
            name[0] == '/' || name[0] == '\\' ||
            (len >= 2 && name[1] == ':' &&
             ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z')))) {
        return false;
    }

    for (size_t i = 0; i < len;) {
        uint8_t c = name[i];
        if (c == 0 || c < 0x20 || c == 0x7f || c == '/' || c == '\\') {
            return false;
        }
        if (c == '.' && i + 1 < len && name[i + 1] == '.') {
            return false;
        }
        if (c < 0x80) {
            ++i;
            continue;
        }

        size_t continuation;
        uint32_t codepoint;
        if (c >= 0xc2 && c <= 0xdf) {
            continuation = 1;
            codepoint = c & 0x1f;
        } else if (c >= 0xe0 && c <= 0xef) {
            continuation = 2;
            codepoint = c & 0x0f;
        } else if (c >= 0xf0 && c <= 0xf4) {
            continuation = 3;
            codepoint = c & 0x07;
        } else {
            return false;
        }
        if (i + continuation >= len) {
            return false;
        }
        for (size_t j = 1; j <= continuation; ++j) {
            uint8_t next = name[i + j];
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((continuation == 2 && codepoint < 0x800) ||
                (continuation == 3 && codepoint < 0x10000) ||
                (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
                codepoint > 0x10ffff) {
            return false;
        }
        i += continuation + 1;
    }
    return true;
}

esp_err_t ft_protocol_encode_meta_request(const ft_meta_request_t *msg, uint8_t *buf,
                                          size_t buf_size, size_t *encoded_len)
{
    if (!msg || !buf || !encoded_len || msg->transfer_id == 0 || msg->block_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t name_len = strnlen(msg->file_name, sizeof(msg->file_name));
    if (!utf8_name_valid((const uint8_t *)msg->file_name, name_len)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t blocks = msg->file_size == 0 ? 0 : 1 + (msg->file_size - 1) / msg->block_size;
    if (blocks > UINT32_MAX || msg->total_blocks != (uint32_t)blocks) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t total = ESP_FT_META_REQ_HDR_LEN + name_len;
    if (buf_size < total) {
        return ESP_ERR_INVALID_SIZE;
    }

    put_be32(buf, msg->transfer_id);
    put_be64(buf + 4, msg->file_size);
    put_be32(buf + 12, msg->block_size);
    put_be32(buf + 16, msg->total_blocks);
    memcpy(buf + 20, msg->sha256, sizeof(msg->sha256));
    put_be16(buf + 52, 0);
    buf[54] = (uint8_t)name_len;
    buf[55] = 0;
    memcpy(buf + ESP_FT_META_REQ_HDR_LEN, msg->file_name, name_len);
    *encoded_len = total;
    return ESP_OK;
}

esp_err_t ft_protocol_decode_meta_request(const uint8_t *buf, size_t len, ft_meta_request_t *msg)
{
    if (!buf || !msg || len < ESP_FT_META_REQ_HDR_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t name_len = buf[54];
    if (name_len == 0 || name_len > ESP_FT_FILE_NAME_MAX_LEN ||
            len != ESP_FT_META_REQ_HDR_LEN + name_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    memset(msg, 0, sizeof(*msg));
    msg->transfer_id = get_be32(buf);
    msg->file_size = get_be64(buf + 4);
    msg->block_size = get_be32(buf + 12);
    msg->total_blocks = get_be32(buf + 16);
    memcpy(msg->sha256, buf + 20, sizeof(msg->sha256));
    memcpy(msg->file_name, buf + ESP_FT_META_REQ_HDR_LEN, name_len);
    if (get_be16(buf + 52) != 0 || buf[55] != 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!utf8_name_valid(buf + ESP_FT_META_REQ_HDR_LEN, name_len)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (msg->transfer_id == 0 || msg->block_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t blocks = msg->file_size == 0 ? 0 : 1 + (msg->file_size - 1) / msg->block_size;
    if (blocks > UINT32_MAX || msg->total_blocks != (uint32_t)blocks) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t ft_protocol_encode_meta_response(const ft_meta_response_t *msg, uint8_t *buf,
                                           size_t buf_size)
{
    if (!msg || !buf || buf_size < ESP_FT_META_RSP_LEN || msg->transfer_id == 0 ||
            !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool accepted = msg->status == ESP_FT_WIRE_STATUS_OK;
    if ((!accepted && msg->status != ESP_FT_WIRE_STATUS_REJECTED) ||
            (accepted && (msg->reason_code != ESP_FT_REASON_OK || msg->accepted_block_size == 0)) ||
            (!accepted && (msg->reason_code == ESP_FT_REASON_OK || msg->accepted_block_size != 0))) {
        return ESP_ERR_INVALID_ARG;
    }
    put_be32(buf, msg->transfer_id);
    buf[4] = msg->status;
    buf[5] = 0;
    put_be16(buf + 6, msg->reason_code);
    put_be32(buf + 8, msg->accepted_block_size);
    return ESP_OK;
}

esp_err_t ft_protocol_decode_meta_response(const uint8_t *buf, size_t len, ft_meta_response_t *msg)
{
    if (!buf || !msg || len != ESP_FT_META_RSP_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    msg->transfer_id = get_be32(buf);
    msg->status = buf[4];
    msg->reason_code = get_be16(buf + 6);
    msg->accepted_block_size = get_be32(buf + 8);
    if (msg->transfer_id == 0 || buf[5] != 0 || !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool accepted = msg->status == ESP_FT_WIRE_STATUS_OK;
    if ((!accepted && msg->status != ESP_FT_WIRE_STATUS_REJECTED) ||
            (accepted && (msg->reason_code != ESP_FT_REASON_OK || msg->accepted_block_size == 0)) ||
            (!accepted && (msg->reason_code == ESP_FT_REASON_OK || msg->accepted_block_size != 0))) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

void ft_protocol_encode_data_request_header(uint8_t buf[ESP_FT_DATA_REQ_HDR_LEN],
                                            uint32_t transfer_id, uint32_t block_index,
                                            uint32_t data_len)
{
    put_be32(buf, transfer_id);
    put_be32(buf + 4, block_index);
    put_be32(buf + 8, data_len);
}

esp_err_t ft_protocol_decode_data_request(const uint8_t *buf, size_t len, ft_data_request_t *msg)
{
    if (!buf || !msg || len < ESP_FT_DATA_REQ_HDR_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    msg->transfer_id = get_be32(buf);
    msg->block_index = get_be32(buf + 4);
    msg->data_len = get_be32(buf + 8);
    if (msg->transfer_id == 0 || msg->data_len == 0 ||
            msg->data_len != len - ESP_FT_DATA_REQ_HDR_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    msg->data = buf + ESP_FT_DATA_REQ_HDR_LEN;
    return ESP_OK;
}

esp_err_t ft_protocol_encode_data_response(const ft_data_response_t *msg, uint8_t *buf,
                                           size_t buf_size)
{
    if (!msg || !buf || buf_size < ESP_FT_DATA_RSP_LEN || msg->transfer_id == 0 ||
            !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool success = msg->status == ESP_FT_WIRE_STATUS_OK;
    if ((!success && msg->status != ESP_FT_WIRE_STATUS_FAILED) ||
            (success && msg->reason_code != ESP_FT_REASON_OK) ||
            (!success && msg->reason_code == ESP_FT_REASON_OK)) {
        return ESP_ERR_INVALID_ARG;
    }
    put_be32(buf, msg->transfer_id);
    put_be32(buf + 4, msg->block_index);
    buf[8] = msg->status;
    buf[9] = 0;
    put_be16(buf + 10, msg->reason_code);
    return ESP_OK;
}

esp_err_t ft_protocol_decode_data_response(const uint8_t *buf, size_t len, ft_data_response_t *msg)
{
    if (!buf || !msg || len != ESP_FT_DATA_RSP_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    msg->transfer_id = get_be32(buf);
    msg->block_index = get_be32(buf + 4);
    msg->status = buf[8];
    msg->reason_code = get_be16(buf + 10);
    if (msg->transfer_id == 0 || buf[9] != 0 || !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool success = msg->status == ESP_FT_WIRE_STATUS_OK;
    if ((!success && msg->status != ESP_FT_WIRE_STATUS_FAILED) ||
            (success && msg->reason_code != ESP_FT_REASON_OK) ||
            (!success && msg->reason_code == ESP_FT_REASON_OK)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t ft_protocol_encode_final_confirm(const ft_final_confirm_t *msg, uint8_t *buf,
                                           size_t buf_size, size_t *encoded_len)
{
    if (!msg || !buf || !encoded_len || msg->transfer_id == 0 ||
            !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t name_len = strnlen(msg->saved_name, sizeof(msg->saved_name));
    if (name_len && !utf8_name_valid((const uint8_t *)msg->saved_name, name_len)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool success = msg->status == ESP_FT_WIRE_STATUS_OK;
    if ((!success && msg->status != ESP_FT_WIRE_STATUS_FAILED) ||
            (success && msg->reason_code != ESP_FT_REASON_OK) ||
            (!success && msg->reason_code == ESP_FT_REASON_OK) ||
            buf_size < ESP_FT_FINAL_HDR_LEN + name_len) {
        return ESP_ERR_INVALID_ARG;
    }
    put_be32(buf, msg->transfer_id);
    buf[4] = msg->status;
    buf[5] = (uint8_t)name_len;
    put_be16(buf + 6, msg->reason_code);
    put_be64(buf + 8, msg->received_size);
    memcpy(buf + 16, msg->received_sha256, sizeof(msg->received_sha256));
    memcpy(buf + ESP_FT_FINAL_HDR_LEN, msg->saved_name, name_len);
    *encoded_len = ESP_FT_FINAL_HDR_LEN + name_len;
    return ESP_OK;
}

esp_err_t ft_protocol_decode_final_confirm(const uint8_t *buf, size_t len, ft_final_confirm_t *msg)
{
    if (!buf || !msg || len < ESP_FT_FINAL_HDR_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t name_len = buf[5];
    if (name_len > ESP_FT_FILE_NAME_MAX_LEN || len != ESP_FT_FINAL_HDR_LEN + name_len ||
            (name_len && !utf8_name_valid(buf + ESP_FT_FINAL_HDR_LEN, name_len))) {
        return ESP_ERR_INVALID_SIZE;
    }
    memset(msg, 0, sizeof(*msg));
    msg->transfer_id = get_be32(buf);
    msg->status = buf[4];
    msg->reason_code = get_be16(buf + 6);
    msg->received_size = get_be64(buf + 8);
    memcpy(msg->received_sha256, buf + 16, sizeof(msg->received_sha256));
    memcpy(msg->saved_name, buf + ESP_FT_FINAL_HDR_LEN, name_len);
    if (msg->transfer_id == 0 || !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    bool success = msg->status == ESP_FT_WIRE_STATUS_OK;
    if ((!success && msg->status != ESP_FT_WIRE_STATUS_FAILED) ||
            (success && msg->reason_code != ESP_FT_REASON_OK) ||
            (!success && msg->reason_code == ESP_FT_REASON_OK)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t ft_protocol_encode_abort(const ft_abort_t *msg, uint8_t *buf, size_t buf_size)
{
    if (!msg || !buf || buf_size < ESP_FT_ABORT_LEN || msg->transfer_id == 0 ||
            msg->reason_code == ESP_FT_REASON_OK || !reason_valid(msg->reason_code)) {
        return ESP_ERR_INVALID_ARG;
    }
    put_be32(buf, msg->transfer_id);
    put_be16(buf + 4, msg->reason_code);
    put_be16(buf + 6, 0);
    return ESP_OK;
}

esp_err_t ft_protocol_decode_abort(const uint8_t *buf, size_t len, ft_abort_t *msg)
{
    if (!buf || !msg || len != ESP_FT_ABORT_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    msg->transfer_id = get_be32(buf);
    msg->reason_code = get_be16(buf + 4);
    if (msg->transfer_id == 0 || msg->reason_code == ESP_FT_REASON_OK ||
            !reason_valid(msg->reason_code) || get_be16(buf + 6) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}
