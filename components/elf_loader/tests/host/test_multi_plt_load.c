/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Execute the real Xtensa loader/validator/relocator with a cross-built
 * four-import .so, without attempting to execute Xtensa instructions on host. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/esp_elf.c"

void *esp_elf_malloc(uint32_t bytes, bool executable)
{
    (void)executable;
    return malloc(bytes);
}

void esp_elf_free(void *ptr)
{
    free(ptr);
}

uintptr_t elf_find_sym_default(const char *name)
{
    const char *const symbols[] = {"malloc", "strlen", "printf", "free"};
    for (unsigned i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i) {
        if (strcmp(name, symbols[i]) == 0) {
            return 0x10000000U + i * 4;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *fp = fopen(argv[1], "rb");
    assert(fp);
    assert(fseek(fp, 0, SEEK_END) == 0);
    long file_size = ftell(fp);
    assert(file_size > 0 && file_size <= INT32_MAX);
    rewind(fp);
    uint8_t *pbuf = malloc((size_t)file_size);
    assert(pbuf);
    assert(fread(pbuf, 1, (size_t)file_size, fp) == (size_t)file_size);
    fclose(fp);

    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    const elf32_shdr_t *shdr = (const elf32_shdr_t *)(pbuf + ehdr->shoff);
    const char *names = (const char *)pbuf + shdr[ehdr->shstrndx].offset;
    const elf32_shdr_t *got = NULL;
    const elf32_shdr_t *text = NULL;
    const elf32_shdr_t *rodata = NULL;
    for (unsigned i = 0; i < ehdr->shnum; ++i) {
        const char *name = names + shdr[i].name;
        if (strcmp(name, ".got.loc") == 0) {
            got = &shdr[i];
        }
        if (strcmp(name, ".text") == 0) {
            text = &shdr[i];
        }
        if (strcmp(name, ".rodata") == 0) {
            rodata = &shdr[i];
        }
    }
    assert(got && text && rodata);
    assert(got->size >= 12);  /* three or more imported PLT entries */

    esp_elf_t elf;
    assert(esp_elf_init(&elf) == 0);
    assert(esp_elf_relocate(&elf, pbuf, (size_t)file_size) == 0);
    assert(elf.entry == NULL);  /* entry-less .so may only be queried */
    assert(elf.text_alloc_size == got->addr + got->size - text->addr);
    assert(elf.sec[ELF_SEC_GOT_LOC].size == got->size);
    assert(elf.sec[ELF_SEC_GOT_LOC].addr ==
           elf.sec[ELF_SEC_TEXT].addr + got->addr - text->addr);
    assert(memcmp((void *)elf.sec[ELF_SEC_GOT_LOC].addr,
                  pbuf + got->offset, got->size) == 0);
    assert(esp_elf_map_sym(&elf, rodata->addr) == elf.sec[ELF_SEC_RODATA].addr);
    assert(esp_elf_map_sym(&elf, rodata->addr) !=
           elf.sec[ELF_SEC_TEXT].addr + rodata->addr - text->addr);

    bool found = false;
    for (unsigned i = 0; i < elf.num; ++i) {
        if (strcmp(elf.symtab[i].name, "plugin_entry") == 0) {
            assert(elf.symtab[i].addr != NULL);
            found = true;
        }
    }
    assert(found);
    esp_elf_deinit(&elf);
    free(pbuf);
    return 0;
}
