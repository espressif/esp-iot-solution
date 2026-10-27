/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "esp_elf.h"
#include "private/elf_platform.h"

int main(void)
{
    uint32_t segment[4] = {0};
    esp_elf_t elf = {
        .psegment = (uint8_t *)segment,
        .svaddr = 0x1000,
        .segment_size = sizeof(segment),
    };
    elf32_rela_t rela = {
        .offset = 0x100c,
        .info = ELF_R_INFO(0, 5), /* R_RISCV_JUMP_SLOT */
        .addend = 0,
    };

    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0x12345678) == 0);
    assert(segment[3] == 0x12345678);

    rela.offset = 0x1004;
    rela.info = ELF_R_INFO(0, 3); /* R_RISCV_RELATIVE */
    rela.addend = 0x1020;
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) == 0);
    uint32_t expected_relative = (uint32_t)((uintptr_t)segment - elf.svaddr + rela.addend);
    assert(segment[1] == expected_relative);

    /* A word-aligned VA can map to an unaligned host pointer. */
    elf.svaddr = 0x1001;
    rela.offset = 0x1004;
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) == -EINVAL);
    elf.svaddr = 0x1000;
    rela.offset = 0x1004;
    /* Also defend against an allocator returning a non-word-aligned base. */
    elf.psegment = (uint8_t *)segment + 1;
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) == -EINVAL);
    elf.psegment = (uint8_t *)segment;
    rela.offset = 0x1010; /* past allocation */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    rela.offset = 0x0ffc; /* before svaddr */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    rela.offset = 0x100d; /* unaligned and straddling end */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    rela.offset = 0x1000;
    rela.info = ELF_R_INFO(0, 0xff); /* unsupported relocation */
    assert(esp_elf_arch_relocate(&elf, &rela, NULL, 0) < 0);
    assert(segment[0] == 0 && segment[1] == expected_relative && segment[2] == 0);
    assert(segment[3] == 0x12345678);
    return 0;
}
