/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_frame.h"
#include "esp_gmp_types.h"
#include <string.h>

static uint16_t rd_be16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

static void wr_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

int esp_gmp_frame_parse(const uint8_t *data, size_t len, esp_gmp_parsed_t *out)
{
    memset(out, 0, sizeof(*out));
    if (len < 10) {
        return -1;
    }

    uint8_t ver_op = data[0];
    out->ver = ver_op >> 4;
    out->op = ver_op & 0x0F;
    out->group_id = data[1];
    out->seq_host = rd_be16(&data[2]);
    out->reserved0 = data[4];
    out->command_id = data[5];
    out->flags = data[6];
    out->status_rsv = data[7];
    out->packet_length_host = rd_be16(&data[8]);
    out->payload = &data[10];
    out->payload_len = len > 10 ? len - 10 : 0;
    return 0;
}

size_t esp_gmp_frame_build(uint8_t *buf, size_t buf_sz, uint8_t ver, uint8_t op,
                           uint8_t group_id, uint16_t seq_host, uint8_t command_id, uint8_t flags,
                           uint8_t status, const uint8_t *payload, size_t payload_len)
{
    size_t need = 10 + payload_len;
    if (need > buf_sz) {
        return 0;
    }
    buf[0] = (uint8_t)((ver << 4) | (op & 0x0F));
    buf[1] = group_id;
    wr_be16(&buf[2], seq_host);
    buf[4] = 0x00;
    buf[5] = command_id;
    buf[6] = (uint8_t)(flags & (uint8_t)~ESP_GMP_FLAG_FRAGMENTED);
    buf[7] = status;
    wr_be16(&buf[8], (uint16_t)payload_len);
    if (payload_len && payload) {
        memcpy(&buf[10], payload, payload_len);
    }
    return need;
}
