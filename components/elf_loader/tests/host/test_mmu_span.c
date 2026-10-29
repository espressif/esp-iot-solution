/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdint.h>

#include "private/elf_platform.h"

int main(void)
{
    const uint32_t page = 0x10000;
    assert(esp_elf_mmu_page_span(0x3f800000, 0, page) == 0);
    assert(esp_elf_mmu_page_span(0x3f800000, 1, page) == 1);
    assert(esp_elf_mmu_page_span(0x3f800000, page, page) == 1);
    assert(esp_elf_mmu_page_span(0x3f800001, page, page) == 2);
    assert(esp_elf_mmu_page_span(0x3f80ffff, 2, page) == 2);
    assert(esp_elf_mmu_page_span(0x3f800100, page * 2, page) == 3);
    assert(esp_elf_mmu_page_span(0x3f800000, 1, 3) == 0);
    assert(esp_elf_mmu_page_span(0x3f800000, 1, 0) == 0);
    return 0;
}
