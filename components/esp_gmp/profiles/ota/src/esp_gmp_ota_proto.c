/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_ota_proto.h"
#include <string.h>

static uint16_t rd_be16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

static uint32_t rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
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

bool esp_gmp_ota_ctrl_parse_req(const uint8_t *payload, size_t len, esp_gmp_ota_ctrl_req_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!payload || len < ESP_GMP_OTA_CTRL_START_LEN) {
        return false;
    }

    out->action = payload[0];
    out->session_id = payload[1];
    out->flags = rd_be16(&payload[2]);
    out->image_size = rd_be32(&payload[4]);

    if (out->action == ESP_GMP_OTA_CTRL_ACTION_FINISH) {
        if (len != ESP_GMP_OTA_CTRL_FINISH_LEN) {
            return false;
        }
        if ((out->flags & ESP_GMP_OTA_CTRL_FLAG_SHA256) == 0) {
            return false;
        }
        out->has_sha256 = true;
        memcpy(out->image_sha256, &payload[8], 32);
        return true;
    }

    if (out->action == ESP_GMP_OTA_CTRL_ACTION_START ||
            out->action == ESP_GMP_OTA_CTRL_ACTION_ABORT ||
            out->action == ESP_GMP_OTA_CTRL_ACTION_ERASE) {
        if (len != ESP_GMP_OTA_CTRL_START_LEN) {
            return false;
        }
        if ((out->flags & ~ESP_GMP_OTA_CTRL_FLAG_SHA256) != 0) {
            return false;
        }
        if (out->action != ESP_GMP_OTA_CTRL_ACTION_START) {
            if (out->image_size != 0 || out->flags != 0) {
                return false;
            }
        }
        return true;
    }

    return false;
}

size_t esp_gmp_ota_ctrl_build_req(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_ctrl_req_t *req)
{
    if (!buf || !req) {
        return 0;
    }

    if (req->action == ESP_GMP_OTA_CTRL_ACTION_FINISH) {
        if (buf_sz < ESP_GMP_OTA_CTRL_FINISH_LEN) {
            return 0;
        }
        if (!req->has_sha256 && (req->flags & ESP_GMP_OTA_CTRL_FLAG_SHA256) == 0) {
            return 0;
        }
        buf[0] = req->action;
        buf[1] = req->session_id;
        wr_be16(&buf[2], (uint16_t)(req->flags | ESP_GMP_OTA_CTRL_FLAG_SHA256));
        wr_be32(&buf[4], req->image_size);
        memcpy(&buf[8], req->image_sha256, 32);
        return ESP_GMP_OTA_CTRL_FINISH_LEN;
    }

    if (req->action == ESP_GMP_OTA_CTRL_ACTION_START ||
            req->action == ESP_GMP_OTA_CTRL_ACTION_ABORT ||
            req->action == ESP_GMP_OTA_CTRL_ACTION_ERASE) {
        if (buf_sz < ESP_GMP_OTA_CTRL_START_LEN) {
            return 0;
        }
        if ((req->flags & ~ESP_GMP_OTA_CTRL_FLAG_SHA256) != 0) {
            return 0;
        }
        if (req->action != ESP_GMP_OTA_CTRL_ACTION_START) {
            if (req->image_size != 0 || req->flags != 0) {
                return 0;
            }
        }
        buf[0] = req->action;
        buf[1] = req->session_id;
        wr_be16(&buf[2], req->flags);
        wr_be32(&buf[4], req->image_size);
        return ESP_GMP_OTA_CTRL_START_LEN;
    }

    return 0;
}

size_t esp_gmp_ota_ctrl_build_start_rsp(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_ctrl_rsp_t *rsp)
{
    if (!buf || !rsp || buf_sz < ESP_GMP_OTA_CTRL_RSP_LEN) {
        return 0;
    }
    buf[0] = rsp->session_id;
    wr_be16(&buf[1], rsp->chunk_hint);
    buf[3] = 0;
    return ESP_GMP_OTA_CTRL_RSP_LEN;
}

bool esp_gmp_ota_ctrl_parse_rsp(const uint8_t *payload, size_t len, esp_gmp_ota_ctrl_rsp_t *out)
{
    if (!payload || !out || len < ESP_GMP_OTA_CTRL_RSP_LEN) {
        return false;
    }
    out->session_id = payload[0];
    out->chunk_hint = rd_be16(&payload[1]);
    return true;
}

size_t esp_gmp_ota_data_parse_req(const uint8_t *payload, size_t len, esp_gmp_ota_data_req_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!payload || len < ESP_GMP_OTA_DATA_HDR_LEN) {
        return 0;
    }

    out->session_id = payload[0];
    out->flags = payload[1];
    out->data_len = rd_be16(&payload[2]);
    out->image_offset = rd_be32(&payload[4]);

    if (out->data_len == 0) {
        return 0;
    }
    if ((size_t)ESP_GMP_OTA_DATA_HDR_LEN + out->data_len != len) {
        return 0;
    }

    out->data = &payload[8];
    return len;
}

size_t esp_gmp_ota_data_build_req(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_data_req_t *req)
{
    if (!buf || !req || req->data_len == 0) {
        return 0;
    }

    size_t total = (size_t)ESP_GMP_OTA_DATA_HDR_LEN + req->data_len;
    if (buf_sz < total) {
        return 0;
    }

    buf[0] = req->session_id;
    buf[1] = req->flags;
    wr_be16(&buf[2], req->data_len);
    wr_be32(&buf[4], req->image_offset);
    if (req->data) {
        memcpy(&buf[ESP_GMP_OTA_DATA_HDR_LEN], req->data, req->data_len);
    }
    return total;
}

bool esp_gmp_ota_data_parse_rsp(const uint8_t *payload, size_t len)
{
    (void)payload;
    /* Typical DATA WRITE_RSP has empty payload; GMP status is outside the body. */
    return len == 0;
}

size_t esp_gmp_ota_query_build_rsp(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_query_rsp_t *rsp)
{
    if (!buf || !rsp || buf_sz < ESP_GMP_OTA_QUERY_RSP_LEN) {
        return 0;
    }
    buf[0] = rsp->session_state;
    buf[1] = rsp->active_session_id;
    wr_be32(&buf[2], rsp->bytes_received);
    wr_be32(&buf[6], rsp->bytes_expected);
    return ESP_GMP_OTA_QUERY_RSP_LEN;
}

bool esp_gmp_ota_query_parse_rsp(const uint8_t *payload, size_t len, esp_gmp_ota_query_rsp_t *out)
{
    if (!payload || !out || len < ESP_GMP_OTA_QUERY_RSP_LEN) {
        return false;
    }
    out->session_state = payload[0];
    out->active_session_id = payload[1];
    out->bytes_received = rd_be32(&payload[2]);
    out->bytes_expected = rd_be32(&payload[6]);
    return true;
}
