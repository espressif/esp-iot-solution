/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_GMP_OTA_CTRL_ACTION_START 0
#define ESP_GMP_OTA_CTRL_ACTION_FINISH 1
#define ESP_GMP_OTA_CTRL_ACTION_ABORT 2
#define ESP_GMP_OTA_CTRL_ACTION_ERASE 3

#define ESP_GMP_OTA_CTRL_FLAG_SHA256 0x0001

#define ESP_GMP_OTA_CTRL_START_LEN 8u
#define ESP_GMP_OTA_CTRL_FINISH_LEN 40u
#define ESP_GMP_OTA_CTRL_ABORT_ERASE_LEN 8u

#define ESP_GMP_OTA_CTRL_RSP_LEN 4u
#define ESP_GMP_OTA_DATA_HDR_LEN 8u
#define ESP_GMP_OTA_QUERY_RSP_LEN 10u

/** SPI flash page size; esp_ota_write() is fastest when each call length is a multiple of this. */
#define ESP_GMP_OTA_FLASH_WRITE_ALIGN 256u

#define ESP_GMP_OTA_DATA_FLAG_LAST_CHUNK 0x01

/** Firmware bytes per DATA message from max GMP payload, rounded down to flash write alignment. */
static inline uint16_t esp_gmp_ota_chunk_for_flash(size_t max_gmp_payload)
{
    if (max_gmp_payload <= ESP_GMP_OTA_DATA_HDR_LEN) {
        return 1;
    }
    size_t raw = max_gmp_payload - ESP_GMP_OTA_DATA_HDR_LEN;
    uint16_t aligned = (uint16_t)((raw / ESP_GMP_OTA_FLASH_WRITE_ALIGN) * ESP_GMP_OTA_FLASH_WRITE_ALIGN);
    return aligned > 0 ? aligned : (uint16_t)raw;
}

/** Round an existing chunk hint down to flash write alignment (minimum 1). */
static inline uint16_t esp_gmp_ota_align_chunk(uint16_t chunk)
{
    uint16_t aligned = (uint16_t)((chunk / ESP_GMP_OTA_FLASH_WRITE_ALIGN) * ESP_GMP_OTA_FLASH_WRITE_ALIGN);
    return aligned > 0 ? aligned : 1;
}

#define ESP_GMP_OTA_SESSION_STATE_IDLE 0
#define ESP_GMP_OTA_SESSION_STATE_OPEN 1
#define ESP_GMP_OTA_SESSION_STATE_CLOSING 2

typedef struct {
    uint8_t action;
    uint8_t session_id;
    uint16_t flags;
    uint32_t image_size;
    uint8_t image_sha256[32];
    bool has_sha256;
} esp_gmp_ota_ctrl_req_t;

typedef struct {
    uint8_t session_id;
    uint16_t chunk_hint;
} esp_gmp_ota_ctrl_rsp_t;

typedef struct {
    uint8_t session_id;
    uint8_t flags;
    uint16_t data_len;
    uint32_t image_offset;
    const uint8_t *data;
} esp_gmp_ota_data_req_t;

typedef struct {
    uint8_t session_state;
    uint8_t active_session_id;
    uint32_t bytes_received;
    uint32_t bytes_expected;
} esp_gmp_ota_query_rsp_t;

bool esp_gmp_ota_ctrl_parse_req(const uint8_t *payload, size_t len, esp_gmp_ota_ctrl_req_t *out);
size_t esp_gmp_ota_ctrl_build_req(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_ctrl_req_t *req);
size_t esp_gmp_ota_ctrl_build_start_rsp(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_ctrl_rsp_t *rsp);
bool esp_gmp_ota_ctrl_parse_rsp(const uint8_t *payload, size_t len, esp_gmp_ota_ctrl_rsp_t *out);
size_t esp_gmp_ota_data_parse_req(const uint8_t *payload, size_t len, esp_gmp_ota_data_req_t *out);
size_t esp_gmp_ota_data_build_req(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_data_req_t *req);
bool esp_gmp_ota_data_parse_rsp(const uint8_t *payload, size_t len);
size_t esp_gmp_ota_query_build_rsp(uint8_t *buf, size_t buf_sz, const esp_gmp_ota_query_rsp_t *rsp);
bool esp_gmp_ota_query_parse_rsp(const uint8_t *payload, size_t len, esp_gmp_ota_query_rsp_t *out);

#ifdef __cplusplus
}
#endif
