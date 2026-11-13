/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bootloader_custom_utility.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DDELTA_MAGIC "DDELTA40"

struct ddelta_header {
    char magic[8];
    uint64_t new_file_size;
};

struct ddelta_entry_header {
    uint64_t diff;
    uint64_t extra;
    union {
        int64_t value;
        uint64_t raw;
    } seek;
};

enum ddelta_error {
    DDELTA_EMAGIC = 1,
    DDELTA_EALGO,
    DDELTA_EPATCHIO,
    DDELTA_EOLDIO,
    DDELTA_ENEWIO,
    DDELTA_EPATCHSHORT
};

#ifndef DDELTA_BLOCK_SIZE
#define DDELTA_BLOCK_SIZE (1024)
#endif

int bootloader_diff_ddelta_init(bootloader_custom_ota_params_t *params);

int bootloader_diff_ddelta_input(void *buf, int size);

/** True only after the zero terminator entry and bytes_written == new_file_size. */
bool bootloader_diff_ddelta_is_complete(void);

#ifdef __cplusplus
}
#endif
