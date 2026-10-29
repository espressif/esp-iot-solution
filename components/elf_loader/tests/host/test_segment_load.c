/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Exercise the actual segment loader with a nonzero first PT_LOAD virtual base. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/esp_elf.c"

static uint32_t allocated_bytes;
static size_t freed_allocations;

void *esp_elf_malloc(uint32_t bytes, bool executable)
{
    (void)executable;
    allocated_bytes = bytes;
    return malloc(bytes);
}

void esp_elf_free(void *ptr)
{
    if (ptr) {
        ++freed_allocations;
    }
    free(ptr);
}

static size_t test_mem_source_size(void *ctx)
{
    const uint8_t **p = ctx;
    (void)p;
    return 0;
}

static int test_mem_source_read(void *ctx, size_t offset, void *buf, size_t len)
{
    const uint8_t *pbuf = *(const uint8_t **)ctx;
    memcpy(buf, pbuf + offset, len);
    return 0;
}

static int call_load_segment(esp_elf_t *elf, const uint8_t *pbuf)
{
    const uint8_t *ptr = pbuf;
    elf_source_t src = {
        .size = test_mem_source_size,
        .read = test_mem_source_read,
        .close = NULL,
        .ctx = &ptr,
    };
    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    return esp_elf_load_segment(elf, &src, ehdr);
}

int main(void)
{
    struct test_image {
        elf32_hdr_t ehdr;
        elf32_phdr_t phdr[2];
        uint8_t data[16];
    } image = {0};
    image.ehdr.phoff = sizeof(image.ehdr);
    image.ehdr.phnum = 2;
    image.ehdr.entry = 0x1020;
    image.phdr[0].type = PT_LOAD;
    image.phdr[0].offset = offsetof(struct test_image, data);
    image.phdr[0].vaddr = 0x1000;
    image.phdr[0].filesz = 8;
    image.phdr[0].memsz = 16;
    image.phdr[1].type = PT_LOAD;
    image.phdr[1].offset = offsetof(struct test_image, data) + 8;
    image.phdr[1].vaddr = 0x1020;
    image.phdr[1].filesz = 8;
    image.phdr[1].memsz = 12;

    for (int i = 0; i < 16; ++i) {
        image.data[i] = (uint8_t)(i + 1);
    }

    assert(!esp_elf_file_size_valid(0));
    assert(!esp_elf_file_size_valid(-1));
    assert(esp_elf_file_size_valid(INT32_MAX));
    assert(!esp_elf_file_size_valid((off_t)INT32_MAX + 1));

    esp_elf_t elf = {0};
    assert(esp_elf_runtime_base(&elf) == 0);
    elf.psegment = (uint8_t *)&image;
    assert(esp_elf_runtime_base(&elf) == 0); /* incomplete load */
    elf.psegment = NULL;
    assert(call_load_segment(&elf, (const uint8_t *)&image) == 0);
    assert(elf.svaddr == 0x1000);
    assert(esp_elf_runtime_base(&elf) == (uintptr_t)elf.psegment);
    assert(allocated_bytes == 0x2c);
    assert(elf.segment_size == allocated_bytes);
    assert(memcmp(elf.psegment, image.data, 8) == 0);
    assert(memcmp(elf.psegment + 0x20, image.data + 8, 8) == 0);
    for (size_t i = 8; i < 0x20; ++i) {
        assert(elf.psegment[i] == 0);
    }
    assert(elf.entry == (void *)(elf.psegment + 0x20));
    assert(esp_elf_map_sym(&elf, 0x1000) == (uintptr_t)elf.psegment);
    assert(esp_elf_map_sym(&elf, 0x1020) == (uintptr_t)(elf.psegment + 0x20));
    assert(esp_elf_map_sym(&elf, 0x0fff) == 0);
    assert(esp_elf_map_sym(&elf, 0x102c) == 0);
    esp_elf_free(elf.psegment);

    /* High virtual addresses must be rebased before forming any host pointer. */
    image.ehdr.entry = 0xfffff020;
    image.phdr[0].vaddr = 0xfffff000;
    image.phdr[1].vaddr = 0xfffff020;
    esp_elf_t high_base = {0};
    assert(call_load_segment(&high_base, (const uint8_t *)&image) == 0);
    assert(high_base.svaddr == 0xfffff000);
    assert(high_base.entry == (void *)(high_base.psegment + 0x20));
    assert(memcmp(high_base.psegment, image.data, 8) == 0);
    assert(memcmp(high_base.psegment + 0x20, image.data + 8, 8) == 0);
    esp_elf_free(high_base.psegment);

    /* ET_EXEC can use zero as a real virtual entry address. */
    image.ehdr.type = ELF_ET_EXEC;
    image.ehdr.entry = 0;
    image.phdr[0].vaddr = 0;
    image.phdr[1].vaddr = 0x20;
    esp_elf_t zero_entry = {0};
    assert(call_load_segment(&zero_entry, (const uint8_t *)&image) == 0);
    assert(zero_entry.svaddr == 0);
    assert(zero_entry.entry == (void *)zero_entry.psegment);
    assert(zero_entry.entry != NULL);
    esp_elf_free(zero_entry.psegment);

    /* An entry-less ET_DYN is treated as a shared library only when enabled. */
    image.ehdr.type = ELF_ET_DYN;
    esp_elf_t shared = {0};
    assert(call_load_segment(&shared, (const uint8_t *)&image) == 0);
#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    assert(shared.entry == NULL);
    assert(esp_elf_request(&shared, 0, 0, NULL) < 0);
#else
    assert(shared.entry == (void *)shared.psegment);
#endif
    esp_elf_free(shared.psegment);

    /* An ET_DYN that has an entry point must not be treated as entry-less. */
    image.ehdr.entry = 0x20;
    esp_elf_t dynamic_with_entry = {0};
    assert(call_load_segment(&dynamic_with_entry, (const uint8_t *)&image) == 0);
    assert(dynamic_with_entry.entry == (void *)(dynamic_with_entry.psegment + 0x20));
    esp_elf_free(dynamic_with_entry.psegment);
#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    /* A first export-name allocation failure resets num to zero but leaves
     * the allocated symbol table owned by the ELF. Deinit must free it. */
    esp_elf_t first_failed_name = {0};
    first_failed_name.symtab = calloc(1, sizeof(esp_symtab_t));
    assert(first_failed_name.symtab);
    first_failed_name.num = 0;
    size_t before_free = freed_allocations;
    esp_elf_deinit(&first_failed_name);
    assert(freed_allocations == before_free + 1);
    assert(!first_failed_name.symtab);

    /* Later failures must free both the allocated name and the table. */
    esp_elf_t later_failed_name = {0};
    later_failed_name.symtab = calloc(2, sizeof(esp_symtab_t));
    assert(later_failed_name.symtab);
    later_failed_name.symtab[0].name = malloc(4);
    assert(later_failed_name.symtab[0].name);
    later_failed_name.num = 1;
    before_free = freed_allocations;
    esp_elf_deinit(&later_failed_name);
    assert(freed_allocations == before_free + 2);
#endif
    return 0;
}
