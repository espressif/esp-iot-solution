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

/* Fail if the loader uses byte-oriented libc memcpy/memset to write directly
 * into an executable allocation. Such calls can trap on word-only Xtensa IRAM.
 * Read/copy operations involving ordinary memory remain permitted. */
static uint8_t *executable_memory;
static size_t executable_memory_size;
static void *checked_memcpy(void *dest, const void *src, size_t size);
static void *checked_memset(void *dest, int value, size_t size);
/* macOS fortified string.h declares memcpy/memset as macros already. */
#ifdef memcpy
#undef memcpy
#endif
#ifdef memset
#undef memset
#endif

static int test_mem_source_read2(void *ctx, size_t offset, void *buf, size_t len)
{
    const uint8_t *pbuf = *(const uint8_t **)ctx;
    memcpy(buf, pbuf + offset, len);
    return 0;
}

#define memcpy checked_memcpy
#define memset checked_memset
#include "../../src/esp_elf.c"
#undef memcpy
#undef memset

static int call_load_section(esp_elf_t *elf, const uint8_t *pbuf)
{
    const uint8_t *ptr = pbuf;
    elf_source_t src = {
        .size = NULL,
        .read = test_mem_source_read2,
        .close = NULL,
        .ctx = &ptr,
    };
    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    const elf32_shdr_t *shdr = (const elf32_shdr_t *)(pbuf + ehdr->shoff);
    const char *shstrab = (const char *)pbuf + shdr[ehdr->shstrndx].offset;
    return esp_elf_load_section(elf, &src, ehdr, shdr, shstrab);
}

static void assert_not_executable_destination(void *dest)
{
    uintptr_t base = (uintptr_t)executable_memory;
    uintptr_t addr = (uintptr_t)dest;
    assert(!executable_memory || !(addr >= base && addr - base < executable_memory_size));
}

static void *checked_memcpy(void *dest, const void *src, size_t size)
{
    assert_not_executable_destination(dest);
    return memcpy(dest, src, size);
}

static void *checked_memset(void *dest, int value, size_t size)
{
    assert_not_executable_destination(dest);
    return memset(dest, value, size);
}

void *esp_elf_malloc(uint32_t bytes, bool executable)
{
    void *ptr = malloc(bytes);
    if (ptr && executable) {
        assert(!executable_memory);
        executable_memory = ptr;
        executable_memory_size = bytes;
    }
    return ptr;
}

void esp_elf_free(void *ptr)
{
    if (ptr && ptr == executable_memory) {
        executable_memory = NULL;
        executable_memory_size = 0;
    }
    free(ptr);
}

#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
/* Emulate the 32-bit ESP32-S3 instruction-bus alias mapping. */
uintptr_t elf_remap_text(esp_elf_t *elf, uintptr_t address)
{
    const esp_elf_sec_t *text = &elf->sec[ELF_SEC_TEXT];
    /* The ESP32 uses 32-bit pointers, unlike this 64-bit host test. */
    uint32_t code = (uint32_t)address;
    uint32_t base = (uint32_t)text->addr;
    if (code >= base && code - base < text->size) {
        return (uintptr_t)(code + 0x1000);
    }
    return address;
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
assert(call_load_section(&executable, (const uint8_t *)&image) == 0);
    assert(executable.ptext != NULL);
    uintptr_t expected_runtime_base = (uintptr_t)executable.ptext;
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    expected_runtime_base = (uint32_t)expected_runtime_base + 0x1000;
#endif
    assert(esp_elf_runtime_base(&executable) == expected_runtime_base);
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
    assert(call_load_section(&shared, (const uint8_t *)&image) == 0);
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
    assert(call_load_section(&dynamic_with_entry, (const uint8_t *)&image) == 0);
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

    /* .got.loc contains the PC-relative literal pool of a 3+ PLT-entry ELF.
     * Keep its original distance from .text in executable RAM, while .rodata
     * between them must remain independently mapped in data RAM. */
    struct got_loc_image {
        elf32_hdr_t ehdr;
        elf32_shdr_t shdr[5];
        char names[sizeof("\0.text\0.rodata\0.got.loc\0.shstrtab")];
        uint32_t text[4];
        uint32_t rodata[4];
        uint32_t got_loc[3];
    } got_image = {0};
    memcpy(got_image.names, "\0.text\0.rodata\0.got.loc\0.shstrtab", sizeof(got_image.names));
    got_image.ehdr.shoff = offsetof(struct got_loc_image, shdr);
    got_image.ehdr.shnum = 5;
    got_image.ehdr.shstrndx = 4;
    got_image.ehdr.entry = 0x1000;
    got_image.ehdr.type = ELF_ET_EXEC;
    got_image.shdr[1] = (elf32_shdr_t) {
        .name = 1, .type = SHT_PROGBITS, .flags = SHF_ALLOC | SHF_EXECINSTR,
        .addr = 0x1000, .size = sizeof(got_image.text),
        .offset = offsetof(struct got_loc_image, text),
    };
    got_image.shdr[2] = (elf32_shdr_t) {
        .name = 1 + sizeof(".text"), .type = SHT_PROGBITS, .flags = SHF_ALLOC,
        .addr = 0x1020, .size = sizeof(got_image.rodata),
        .offset = offsetof(struct got_loc_image, rodata),
    };
    got_image.shdr[3] = (elf32_shdr_t) {
        .name = 1 + sizeof(".text") + sizeof(".rodata"),
        .type = SHT_PROGBITS, .flags = SHF_ALLOC, .addr = 0x1040,
        .size = sizeof(got_image.got_loc),
        .offset = offsetof(struct got_loc_image, got_loc),
    };
    got_image.shdr[4] = (elf32_shdr_t) {
        .name = 1 + sizeof(".text") + sizeof(".rodata") + sizeof(".got.loc"),
        .type = SHT_STRTAB, .offset = offsetof(struct got_loc_image, names),
        .size = sizeof(got_image.names),
    };
    got_image.rodata[0] = 0xfeedbeef;
    got_image.got_loc[0] = 0x1020; /* R_XTENSA_RELATIVE literal to .rodata */
    got_image.got_loc[1] = 0x11223344;
    got_image.got_loc[2] = 0x55667788;

    esp_elf_t got_elf = {0};
    assert(call_load_section(&got_elf, (const uint8_t *)&got_image) == 0);
    assert(got_elf.text_alloc_size == 0x4c);
    assert(got_elf.sec[ELF_SEC_TEXT].size == sizeof(got_image.text));
    assert(got_elf.sec[ELF_SEC_GOT_LOC].addr == (uintptr_t)got_elf.ptext + 0x40);
    assert(memcmp(got_elf.ptext + 0x40, got_image.got_loc, sizeof(got_image.got_loc)) == 0);
    assert(esp_elf_map_sym(&got_elf, 0x1040) == (uintptr_t)(got_elf.ptext + 0x40));
    assert(esp_elf_map_sym(&got_elf, 0x1020) == got_elf.sec[ELF_SEC_RODATA].addr);
    assert(esp_elf_map_sym(&got_elf, 0x1020) != (uintptr_t)(got_elf.ptext + 0x20));
    assert(esp_elf_map_sym(&got_elf, 0x1030) == 0);
    assert(esp_elf_map_sym(&got_elf, 0x104c) == 0);
    assert(memcmp((const void *)got_elf.sec[ELF_SEC_RODATA].addr,
                  got_image.rodata, sizeof(got_image.rodata)) == 0);
    /* R_XTENSA_RELATIVE (type 5) must be able to write a .got.loc word. */
    elf32_rela_t rela = { .info = ELF_R_INFO(0, 5), .offset = 0x1040 };
    assert(esp_elf_arch_relocate(&got_elf, &rela, NULL, 0) == 0);
    assert(((uint32_t *)(void *)(got_elf.ptext + 0x40))[0] ==
           (uint32_t)got_elf.sec[ELF_SEC_RODATA].addr);
    esp_elf_deinit(&got_elf);

    /* A non-word-aligned .text and .got.loc must be padded safely without
     * writing beyond the executable allocation or reading beyond the image. */
    got_image.shdr[1].size = 3;
    got_image.shdr[3].size = 7;
    esp_elf_t partial = {0};
    assert(call_load_section(&partial, (const uint8_t *)&got_image) == 0);
    assert(partial.text_alloc_size == 0x48);
    assert(memcmp(partial.ptext, got_image.text, 3) == 0);
    assert(partial.ptext[3] == 0);
    assert(memcmp(partial.ptext + 0x40, got_image.got_loc, 7) == 0);
    assert(partial.ptext[0x47] == 0);
    esp_elf_deinit(&partial);
    got_image.shdr[1].size = sizeof(got_image.text);
    got_image.shdr[3].size = sizeof(got_image.got_loc);

    /* A literal pool placed before/inside .text or with a misaligned
     * PC-relative offset must be rejected, not allocate an invalid span. */
    got_image.shdr[3].addr = 0x1008;
    esp_elf_t overlapping = {0};
    assert(call_load_section(&overlapping, (const uint8_t *)&got_image) == -EINVAL);
    assert(overlapping.ptext == NULL);
    got_image.shdr[3].addr = 0x1042;
    esp_elf_t unaligned = {0};
    assert(call_load_section(&unaligned, (const uint8_t *)&got_image) == -EINVAL);
    assert(unaligned.ptext == NULL);

    return 0;
}
