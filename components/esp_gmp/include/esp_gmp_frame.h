/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t ver;
    uint8_t op;
    uint8_t group_id;
    uint16_t seq_host;
    uint8_t reserved0;
    uint8_t command_id;
    uint8_t flags;
    uint8_t status_rsv;
    uint16_t packet_length_host;
    const uint8_t *payload;
    size_t payload_len;
} esp_gmp_parsed_t;

int esp_gmp_frame_parse(const uint8_t *data, size_t len, esp_gmp_parsed_t *out);

size_t esp_gmp_frame_build(uint8_t *buf, size_t buf_sz, uint8_t ver, uint8_t op,
                           uint8_t group_id, uint16_t seq_host, uint8_t command_id, uint8_t flags,
                           uint8_t status, const uint8_t *payload, size_t payload_len);
