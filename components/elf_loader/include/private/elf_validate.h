/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_elf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Validate an entire ELF image before the loader dereferences any of its tables. */
int esp_elf_validate(const uint8_t *pbuf, size_t size);

/* Same structural checks as esp_elf_validate(), reading each header or
 * table on demand through the source. Bounds every read the loader will
 * make, so a malformed image is rejected before any dereference. */
int esp_elf_validate_source(const elf_source_t *src);

#ifdef __cplusplus
}
#endif
