/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h>
#include "private/elf_types.h"

/* Keep the validator and section loader in agreement about which ELF sections
 * contain startup/shutdown callbacks. Only load allocated, nonempty arrays. */
static inline int esp_elf_lifecycle_section_kind(const elf32_shdr_t *shdr,
                                                 const char *name, bool *reverse)
{
    *reverse = false;
    if (!(shdr->flags & SHF_ALLOC) || !shdr->size) {
        return -1;
    }
    if (shdr->type == SHT_PREINIT_ARRAY) {
        return ELF_SEC_PREINIT;
    }
    if (shdr->type == SHT_INIT_ARRAY) {
        return ELF_SEC_INIT;
    }
    if (shdr->type == SHT_FINI_ARRAY) {
        *reverse = true;
        return ELF_SEC_FINI;
    }
    if (shdr->type == SHT_PROGBITS && strcmp(name, ".ctors") == 0) {
        *reverse = true;
        return ELF_SEC_INIT;
    }
    if (shdr->type == SHT_PROGBITS && strcmp(name, ".dtors") == 0) {
        return ELF_SEC_FINI;
    }
    return -1;
}
