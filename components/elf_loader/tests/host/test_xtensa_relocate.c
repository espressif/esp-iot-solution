/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdint.h>

#include "esp_elf.h"
#include "private/elf_platform.h"

/* Minimal host section mapper; the loader's section layout is validated separately. */
uintptr_t esp_elf_map_sym(esp_elf_t *elf, uintptr_t sym)
{
    for (size_t i = 0; i < ELF_SECS; ++i) {
        if (sym >= elf->sec[i].v_addr &&
                sym - elf->sec[i].v_addr < elf->sec[i].size) {
            return sym - elf->sec[i].v_addr + elf->sec[i].addr;
        }
    }
    return 0;
}

int main(void)
{
    uint32_t text[4] = {0};
    esp_elf_t elf = {0};
    elf.sec[ELF_SEC_TEXT].v_addr = 0x2000;
    elf.sec[ELF_SEC_TEXT].addr = (uintptr_t)text;
    elf.sec[ELF_SEC_TEXT].size = sizeof(text);

    elf32_rela_t rela = {
        .offset = 0x2004,
        .info = ELF_R_INFO(0, 4), /* R_XTENSA_JMP_SLOT */
    };

    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0x12345678) == 0);
    assert(text[1] == 0x12345678);

    /* Aligned offset, but only 2 bytes remain in the loaded section. */
    elf.sec[ELF_SEC_TEXT].size = 6;
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    assert(text[1] == 0x12345678);
    elf.sec[ELF_SEC_TEXT].size = sizeof(text);

    rela.offset = 0x1ffc; /* not mapped */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    rela.offset = 0x2010; /* past allocation */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    rela.offset = 0x2005; /* unaligned */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    rela.offset = 0x2000;
    rela.info = ELF_R_INFO(0, 0xff); /* unsupported */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    assert(text[0] == 0 && text[2] == 0 && text[3] == 0);
    assert(text[1] == 0x12345678);
    return 0;
}
