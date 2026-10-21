/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Exercise the real Xtensa section loader with zero and nonzero ELF entry addresses. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
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

#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
/* Emulate the 32-bit ESP32-S3 instruction-bus alias mapping. */
uintptr_t elf_remap_text(esp_elf_t *elf, uintptr_t address)
{
    (void)elf;
    return (uintptr_t)((uint32_t)address + 0x1000);
}
#endif

int main(void)
{
    struct test_image {
        elf32_hdr_t ehdr;
        elf32_shdr_t shdr[3];
        char names[sizeof("\0.text\0.shstrtab")];
        uint32_t text_word;
    } image = {0};

    memcpy(image.names, "\0.text\0.shstrtab", sizeof(image.names));
    image.text_word = 0x12345678;
    image.ehdr.shoff = offsetof(struct test_image, shdr);
    image.ehdr.shnum = 3;
    image.ehdr.shentsize = sizeof(elf32_shdr_t);
    image.ehdr.shstrndx = 2;
    image.ehdr.type = ELF_ET_EXEC;
    image.ehdr.entry = 0;

    image.shdr[1].name = 1;
    image.shdr[1].type = SHT_PROGBITS;
    image.shdr[1].flags = SHF_ALLOC | SHF_EXECINSTR;
    image.shdr[1].addr = 0;
    image.shdr[1].offset = offsetof(struct test_image, text_word);
    image.shdr[1].size = sizeof(image.text_word);

    image.shdr[2].name = 7;
    image.shdr[2].type = SHT_STRTAB;
    image.shdr[2].offset = offsetof(struct test_image, names);
    image.shdr[2].size = sizeof(image.names);

    /* An ET_EXEC at virtual address zero is not an entry-less shared object. */
    esp_elf_t executable = {0};
    assert(esp_elf_load_section(&executable, (const uint8_t *)&image) == 0);
    assert(executable.ptext != NULL);
    uint32_t expected_zero_entry = (uint32_t)(uintptr_t)executable.ptext;
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    expected_zero_entry += 0x1000;
#endif
    assert(executable.entry == (void *)(uintptr_t)expected_zero_entry);
    assert(executable.entry != NULL);
    assert(memcmp(executable.ptext, &image.text_word, sizeof(image.text_word)) == 0);
    esp_elf_free(executable.ptext);

    /* ET_DYN with entry zero is special only if dynamic loading is configured. */
    image.ehdr.type = ELF_ET_DYN;
    esp_elf_t shared = {0};
    assert(esp_elf_load_section(&shared, (const uint8_t *)&image) == 0);
#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    assert(shared.entry == NULL);
    assert(esp_elf_request(&shared, 0, 0, NULL) < 0);
#else
    uint32_t expected_shared_entry = (uint32_t)(uintptr_t)shared.ptext;
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    expected_shared_entry += 0x1000;
#endif
    assert(shared.entry == (void *)(uintptr_t)expected_shared_entry);
#endif
    esp_elf_free(shared.ptext);

    /* A nonzero ET_DYN entry remains executable in either configuration. */
    image.ehdr.entry = 2;
    esp_elf_t dynamic_with_entry = {0};
    assert(esp_elf_load_section(&dynamic_with_entry, (const uint8_t *)&image) == 0);
    uint32_t entry = (uint32_t)(uintptr_t)dynamic_with_entry.ptext + 2;
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    entry += 0x1000;
#endif
    assert(dynamic_with_entry.entry == (void *)(uintptr_t)entry);
#if defined(CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT) && defined(CONFIG_ELF_LOADER_CACHE_OFFSET)
    /* dlsym() must resolve to the same executable alias as an ELF entry. */
    assert(esp_elf_exported_function_address(&dynamic_with_entry, 2) == dynamic_with_entry.entry);
#endif
    esp_elf_free(dynamic_with_entry.ptext);

    return 0;
}
