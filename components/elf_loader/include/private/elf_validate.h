/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Validate an entire ELF image before the loader dereferences any of its tables. */
int esp_elf_validate(const uint8_t *pbuf, size_t size);

#ifdef __cplusplus
}
#endif
