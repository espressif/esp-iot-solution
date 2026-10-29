/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The I-bus MMU must stop mapping an allocation before its RAM is freed. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../src/esp_elf.c"

static int mmu_detaches;
static int frees;

void *esp_elf_malloc(uint32_t bytes, bool executable)
{
    (void)executable;
    return malloc(bytes);
}

void esp_elf_free(void *ptr)
{
    if (ptr) {
        /* A previous allocation cannot be returned to the heap while it is
         * still mapped as executable memory by the module MMU mapping. */
        assert(mmu_detaches == 1);
        ++frees;
    }
    free(ptr);
}

int esp_elf_arch_init_mmu(esp_elf_t *elf)
{
    (void)elf;
    return 0;
}

void esp_elf_arch_deinit_mmu(esp_elf_t *elf)
{
    assert(elf->ptext != NULL);
    assert(elf->mmu_num == 1);
    assert(mmu_detaches == 0 && frees == 0);
    ++mmu_detaches;
}

int main(void)
{
    esp_elf_t elf = {0};
    elf.ptext = malloc(32);
    elf.pdata = malloc(16);
    assert(elf.ptext && elf.pdata);
    elf.mmu_num = 1;
    elf.mmu_off = 8;
    esp_elf_deinit(&elf);
    assert(mmu_detaches == 1 && frees == 2);
    assert(elf.ptext == NULL && elf.pdata == NULL);
    return 0;
}
