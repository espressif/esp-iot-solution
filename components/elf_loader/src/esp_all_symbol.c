/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "private/elf_symbol.h"

/* Extern declarations from ELF symbol table */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#pragma GCC diagnostic pop

/* Available ELF symbols table: g_customer_elfsyms */

const struct esp_elfsym g_customer_elfsyms[] = {
    ESP_ELFSYM_END
};
