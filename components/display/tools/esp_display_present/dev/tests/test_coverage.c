/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../src/backend/present_coverage.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    for (size_t total = 1; total <= 137; total++) {
        for (size_t start = 0; start <= total; start++) {
            for (size_t count = 0; count <= total - start; count++) {
                uint8_t actual[20], expected[20];
                memset(actual, 0xa5, sizeof(actual));
                memcpy(expected, actual, sizeof(actual));
                for (size_t bit = start; bit < start + count; bit++) {
                    expected[bit / 8] |= 1u << (bit % 8);
                }
                present_coverage_set(actual, start, count);
                assert(!memcmp(actual, expected, sizeof(actual)));
            }
        }
        uint8_t bits[20] = {0};
        present_coverage_set(bits, 0, total);
        assert(present_coverage_full(bits, total));
        for (size_t missing = 0; missing < total; missing++) {
            bits[missing / 8] &= ~(1u << (missing % 8));
            assert(!present_coverage_full(bits, total));
            bits[missing / 8] |= 1u << (missing % 8);
        }
    }
    puts("coverage: unaligned spans, partial bytes, sentinels and every missing bit passed");
}
