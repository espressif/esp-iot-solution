/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test the real relocation loop with shared-object loading enabled and disabled. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/esp_elf.c"

#define SEC_TEXT   1
#define SEC_SHSTR  (SEC_TEXT + sizeof(".text"))
#define SEC_STRTAB (SEC_SHSTR + sizeof(".shstrtab"))
#define SEC_SYMTAB (SEC_STRTAB + sizeof(".strtab"))
#define SEC_RELA   (SEC_SYMTAB + sizeof(".symtab"))
#define STR_LOCAL  1
#define STR_HOST   (STR_LOCAL + sizeof("local"))
#define STR_MISSING (STR_HOST + sizeof("host"))

static uint32_t observed_symbol;

uintptr_t elf_find_sym_default(const char *name)
{
    return strcmp(name, "host") == 0 ? 0x12345678 : 0;
}

int esp_elf_validate(const uint8_t *data, size_t length)
{
    (void)data;
    (void)length;
    /* Test the relocation loop, not the separately covered ELF validator. */
    return 0;
}

void *esp_elf_malloc(uint32_t size, bool exec)
{
    (void)exec;
    return malloc(size);
}

void esp_elf_free(void *ptr)
{
    free(ptr);
}

int esp_elf_arch_relocate(esp_elf_t *elf, const elf32_rela_t *rela,
                          const elf32_sym_t *sym, uint32_t addr)
{
    (void)sym;
    observed_symbol = addr;
    uint32_t *where = (uint32_t *)esp_elf_map_sym(elf, rela->offset);
    assert(where);
    *where = addr;
    return 0;
}

struct test_image {
    elf32_hdr_t ehdr;
    elf32_phdr_t phdr;
    elf32_shdr_t shdr[6];
    char names[sizeof("\0.text\0.shstrtab\0.strtab\0.symtab\0.rela.dyn")];
    char strings[sizeof("\0local\0host\0missing")];
    elf32_sym_t symtab[2];
    elf32_rela_t relocation;
    uint32_t segment[16];
};

static void make_image(struct test_image *image)
{
    memset(image, 0, sizeof(*image));
    memcpy(image->names, "\0.text\0.shstrtab\0.strtab\0.symtab\0.rela.dyn", sizeof(image->names));
    memcpy(image->strings, "\0local\0host\0missing", sizeof(image->strings));

    image->ehdr.type = ELF_ET_EXEC;
    image->ehdr.entry = 0x1000;
    image->ehdr.phoff = offsetof(struct test_image, phdr);
    image->ehdr.phnum = 1;
    image->ehdr.shoff = offsetof(struct test_image, shdr);
    image->ehdr.shnum = 6;
    image->ehdr.shstrndx = 2;

    image->phdr.type = PT_LOAD;
    image->phdr.vaddr = 0x1000;
    image->phdr.offset = offsetof(struct test_image, segment);
    image->phdr.filesz = sizeof(image->segment);
    image->phdr.memsz = sizeof(image->segment);

    image->shdr[1] = (elf32_shdr_t) {
        .name = SEC_TEXT, .type = SHT_PROGBITS,
        .flags = SHF_ALLOC | SHF_EXECINSTR, .addr = 0x1000,
        .offset = offsetof(struct test_image, segment), .size = sizeof(image->segment)
    };
    image->shdr[2] = (elf32_shdr_t) {
        .name = SEC_SHSTR, .type = SHT_STRTAB,
        .offset = offsetof(struct test_image, names), .size = sizeof(image->names)
    };
    image->shdr[3] = (elf32_shdr_t) {
        .name = SEC_STRTAB, .type = SHT_STRTAB,
        .offset = offsetof(struct test_image, strings), .size = sizeof(image->strings)
    };
    image->shdr[4] = (elf32_shdr_t) {
        .name = SEC_SYMTAB, .type = SHT_SYMTAB,
        .offset = offsetof(struct test_image, symtab), .size = sizeof(image->symtab), .link = 3
    };
    image->shdr[5] = (elf32_shdr_t) {
        .name = SEC_RELA, .type = SHT_RELA,
        .offset = offsetof(struct test_image, relocation), .size = sizeof(image->relocation), .link = 4
    };
    image->symtab[1].name = STR_LOCAL;
    image->symtab[1].info = ELF_ST_INFO(STB_WEAK, STT_OBJECT);
    image->symtab[1].shndx = 1;
    image->symtab[1].value = 0x1024;
    image->relocation.info = ELF_R_INFO(1, 1);  /* R_RISCV_32 */
    image->relocation.offset = 0x1004;
}

static void run_case(struct test_image *image, bool success, uint32_t expected_addr, bool local)
{
    esp_elf_t elf = {0};
    observed_symbol = 0;
    int ret = esp_elf_relocate(&elf, (const uint8_t *)image, sizeof(*image));
    if (success) {
        assert(ret == 0);
        uint32_t expected = local ?
                            (uint32_t)(uintptr_t)(elf.psegment + image->symtab[1].value - elf.svaddr) :
                            expected_addr;
        assert(observed_symbol == expected);
        assert(((uint32_t *)elf.psegment)[1] == expected);
    } else {
        assert(ret == -ENOSYS);
        assert(observed_symbol == 0);
    }
    esp_elf_deinit(&elf);
}

int main(void)
{
    struct test_image image;
    make_image(&image);

    /* Weak symbols defined by the module require a fallback independent of dlopen(). */
    run_case(&image, true, 0, true);
    /* Undefined references still resolve from the host when exported. */
    image.symtab[1].name = STR_HOST;
    image.symtab[1].shndx = SHN_UNDEF;
    image.symtab[1].value = 0;
    run_case(&image, true, 0x12345678, false);
    /* Unresolved strong imports still fail; weak imports may resolve to 0. */
    image.symtab[1].name = STR_MISSING;
    image.symtab[1].info = ELF_ST_INFO(STB_GLOBAL, STT_OBJECT);
    run_case(&image, false, 0, false);
    /* Defined symbols outside loaded segments are rejected. */
    image.symtab[1].name = STR_LOCAL;
    image.symtab[1].shndx = 1;
    image.symtab[1].value = 0x9999;
    run_case(&image, false, 0, false);

    /* A named STB_LOCAL symbol must never resolve to a host export with
     * the same name; it belongs to the module even when dlsym is enabled. */
    image.symtab[1].name = STR_HOST;
    image.symtab[1].info = ELF_ST_INFO(STB_LOCAL, STT_OBJECT);
    image.symtab[1].shndx = 1;
    image.symtab[1].value = 0x1024;
    run_case(&image, true, 0, true);

    /* STT_SECTION commonly has an empty name. R_RISCV_32 must still
     * relocate it against the loaded image, not absolute address zero. */
    image.symtab[1].name = 0;
    image.symtab[1].info = ELF_ST_INFO(STB_LOCAL, STT_SECTION);
    run_case(&image, true, 0, true);

    /* An unresolved weak undefined data symbol is specified by ELF to
     * relocate as zero; strong undefined symbols must still fail. */
    image.symtab[1].name = STR_MISSING;
    image.symtab[1].shndx = SHN_UNDEF;
    image.symtab[1].value = 0;
    image.symtab[1].info = ELF_ST_INFO(STB_WEAK, STT_OBJECT);
    run_case(&image, true, 0, false);

    /* The RISC-V COPY-like relocation dispatch (type 4) takes the second
     * resolver branch. A defined ELF symbol may legitimately have vaddr 0,
     * so host lookup fails before the local fallback is applied. */
    image.relocation.info = ELF_R_INFO(1, STT_FILE);
    image.ehdr.entry = 0;
    image.phdr.vaddr = 0;
    image.shdr[1].addr = 0;
    image.relocation.offset = 4;
    image.symtab[1].value = 0;
    image.symtab[1].shndx = 1;
    run_case(&image, true, 0, true);
    return 0;
}
