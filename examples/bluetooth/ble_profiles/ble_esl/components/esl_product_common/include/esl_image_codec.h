/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void esl_image_hex_dump(const char *tag, uint8_t image_index, uint32_t offset,
                        const uint8_t *data, size_t len, size_t bytes_per_line);

#ifdef __cplusplus
}
#endif
