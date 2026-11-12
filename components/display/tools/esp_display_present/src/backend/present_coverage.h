/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Caller has validated the range against its allocation. Bit zero is the LSB.
 * Do not touch unused tail bits: adjacent rows may share the same byte. */
static inline void present_coverage_set(uint8_t *bits, size_t start, size_t count)
{
    if (start % 8 && count) {
        size_t head = 8 - start % 8;
        if (head > count) {
            head = count;
        }
        bits[start / 8] |= (uint8_t)(((1u << head) - 1) << (start % 8));
        start += head;
        count -= head;
    }
    size_t bytes = count / 8;
    memset(bits + start / 8, 0xff, bytes);
    start += bytes * 8;
    count %= 8;
    if (count) {
        bits[start / 8] |= (uint8_t)((1u << count) - 1);
    }
}

static inline bool present_coverage_all_zero(const uint8_t *bits, size_t start, size_t count)
{
    if (start % 8 && count) {
        size_t head = 8 - start % 8;
        if (head > count) {
            head = count;
        }
        if (bits[start / 8] & (uint8_t)(((1u << head) - 1) << (start % 8))) {
            return false;
        }
        start += head;
        count -= head;
    }
    size_t bytes = count / 8;
    for (size_t i = 0; i < bytes; i++) {
        if (bits[start / 8 + i] != 0) {
            return false;
        }
    }
    start += bytes * 8;
    count %= 8;
    if (count) {
        if (bits[start / 8] & (uint8_t)((1u << count) - 1)) {
            return false;
        }
    }
    return true;
}

static inline bool present_coverage_full(const uint8_t *bits, size_t count)
{
    size_t bytes = count / 8;
    for (size_t i = 0; i < bytes; i++) {
        if (bits[i] != 0xff) {
            return false;
        }
    }
    unsigned tail = count % 8;
    uint8_t mask = (uint8_t)((1u << tail) - 1);
    return !tail || (bits[bytes] & mask) == mask;
}
