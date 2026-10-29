/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** Total extended advertising payload size used by the ext-adv demo pair. */
#define EXT_ADV_1K_TOTAL 1000

/** GAP device name string embedded in the payload (also used as GAP name on the advertiser). */
#define EXT_ADV_DEMO_NAME "ESP_EXT_ADV_1K"

/**
 * @brief Build a valid AD payload of exactly EXT_ADV_1K_TOTAL octets (flags, name, manufacturer TLVs).
 */
static inline void ext_adv_1k_demo_fill(uint8_t *buf, size_t cap)
{
    if (cap < EXT_ADV_1K_TOTAL) {
        return;
    }
    memset(buf, 0, cap);
    size_t off = 0;

    buf[off++] = 2;
    buf[off++] = 1;
    buf[off++] = 0x06;

    size_t nl = strlen(EXT_ADV_DEMO_NAME);
    buf[off++] = (uint8_t)(1u + nl);
    buf[off++] = 0x09;
    memcpy(&buf[off], EXT_ADV_DEMO_NAME, nl);
    off += nl;

    while (off < EXT_ADV_1K_TOTAL) {
        size_t rem = EXT_ADV_1K_TOTAL - off;
        if (rem < 3u) {
            while (off < EXT_ADV_1K_TOTAL) {
                buf[off++] = 0;
            }
            break;
        }
        uint8_t L = (uint8_t)((rem - 1u > 255u) ? 255u : (rem - 1u));
        if (L < 3u) {
            while (off < EXT_ADV_1K_TOTAL) {
                buf[off++] = 0;
            }
            break;
        }
        buf[off++] = L;
        buf[off++] = 0xFF;
        size_t pay = (size_t)L - 1u;
        for (size_t j = 0; j < pay && off < EXT_ADV_1K_TOTAL; j++) {
            if (j == 0u) {
                buf[off++] = 0xE5;
            } else if (j == 1u) {
                buf[off++] = 0x02;
            } else {
                size_t pos = off;
                buf[off++] = (uint8_t)((pos + j) ^ (uint8_t)j);
            }
        }
    }
}

/** FNV-1a over the payload (compare scanner vs advertiser logs). */
static inline uint32_t ext_adv_1k_demo_fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}
