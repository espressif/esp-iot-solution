/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Host tests of C++ startup/teardown without executing target ISA code. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/esp_elf.c"

static char trace[32];
static size_t trace_pos;

static void record(char c)
{
    assert(trace_pos + 1 < sizeof(trace));
    trace[trace_pos++] = c;
    trace[trace_pos] = '\0';
}

static void pre1(void)
{
    record('P');
}
static void pre2(void)
{
    record('Q');
}
static void init1(void)
{
    record('A');
}
static void init2(void)
{
    record('B');
}
static void fini1(void)
{
    record('X');
}
static void fini2(void)
{
    record('Y');
}
static int entry(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    record('E');
    return 0;
}

void *esp_elf_malloc(uint32_t size, bool executable)
{
    (void)executable;
    return malloc(size);
}

void esp_elf_free(void *ptr)
{
    free(ptr);
}

static void check_order(bool legacy)
{
    void (*pre[])(void) = {pre1, pre2};
    void (*init[])(void) = {init1, init2};
    void (*fini[])(void) = {fini1, fini2};
    esp_elf_t elf = {
        .entry = entry,
        .preinit_array = pre, .preinit_count = 2,
        .init_array = init, .init_count = 2, .init_reverse = legacy,
        .fini_array = fini, .fini_count = 2, .fini_reverse = !legacy,
    };
    memset(trace, 0, sizeof(trace));
    trace_pos = 0;
    assert(esp_elf_request(&elf, 0, 0, NULL) == 0);
    assert(strcmp(trace, legacy ? "PQBAE" : "PQABE") == 0);
    assert(esp_elf_request(&elf, 0, 0, NULL) == 0);
    assert(strcmp(trace, legacy ? "PQBAEE" : "PQABEE") == 0);
    esp_elf_deinit(&elf);
    assert(strcmp(trace, legacy ? "PQBAEEXY" : "PQABEEYX") == 0);
    assert(!elf.init_done);
    assert(!elf.entry);
    esp_elf_deinit(&elf);
    assert(strcmp(trace, legacy ? "PQBAEEXY" : "PQABEEYX") == 0);
}

static void check_section_mapping(void)
{
    const char names[] = "\0.preinit_array\0.init_array\0.fini_array\0";
    elf32_shdr_t sh[3] = {
        {
            .type = SHT_PREINIT_ARRAY, .flags = SHF_ALLOC | SHF_WRITE,
            .name = 1, .addr = 0x1004, .size = 4
        },
        {
            .type = SHT_INIT_ARRAY, .flags = SHF_ALLOC | SHF_WRITE,
            .name = 1 + sizeof(".preinit_array"), .addr = 0x1008, .size = 4
        },
        {
            .type = SHT_FINI_ARRAY, .flags = SHF_ALLOC | SHF_WRITE,
            .name = 1 + sizeof(".preinit_array") + sizeof(".init_array"),
            .addr = 0x100c, .size = 4
        },
    };
    uint32_t storage[8] = {0};
    esp_elf_t elf = {0};
#ifdef CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    elf.sec[ELF_SEC_PREINIT] = (esp_elf_sec_t) {
        .v_addr = 0x1004, .addr = (uintptr_t)&storage[1], .size = 4
    };
    elf.sec[ELF_SEC_INIT] = (esp_elf_sec_t) {
        .v_addr = 0x1008, .addr = (uintptr_t)&storage[2], .size = 4
    };
    elf.sec[ELF_SEC_FINI] = (esp_elf_sec_t) {
        .v_addr = 0x100c, .addr = (uintptr_t)&storage[3], .size = 4
    };
#else
    elf.psegment = (uint8_t *)storage;
    elf.svaddr = 0x1000;
    elf.segment_size = sizeof(storage);
#endif
    assert(esp_elf_find_lifecycle_arrays(&elf, sh, 3, names) == 0);
    assert(elf.preinit_array == (void (**)(void))&storage[1]);
    assert(elf.init_array == (void (**)(void))&storage[2]);
    assert(elf.fini_array == (void (**)(void))&storage[3]);
    assert(elf.preinit_count == 1 && elf.init_count == 1 && elf.fini_count == 1);
    assert(!elf.init_reverse && elf.fini_reverse);

    sh[1].type = SHT_PROGBITS;
    sh[1].name = 1; /* the name passed below is .ctors */
    sh[2].type = SHT_PROGBITS;
    sh[2].name = 1 + sizeof(".ctors");
    const char legacy[] = "\0.ctors\0.dtors\0";
    assert(esp_elf_find_lifecycle_arrays(&elf, sh + 1, 2, legacy) == 0);
    assert(elf.init_reverse && !elf.fini_reverse);

    /* Even a validated-looking array must be fully mapped to one region. */
    sh[1].type = SHT_INIT_ARRAY;
    sh[1].addr = 0x2000;
    assert(esp_elf_find_lifecycle_arrays(&elf, sh + 1, 1, legacy) == -EINVAL);
    sh[1].addr = 0x1008;
    sh[1].size = 3;
    assert(esp_elf_find_lifecycle_arrays(&elf, sh + 1, 1, legacy) == -EINVAL);
    sh[1].size = 4;
    sh[1].addr = 0x1009;
    assert(esp_elf_find_lifecycle_arrays(&elf, sh + 1, 1, legacy) == -EINVAL);
}

int main(void)
{
    check_order(false);  /* .init_array ascending; .fini_array descending */
    check_order(true);   /* legacy .ctors descending; .dtors ascending */
    check_section_mapping();
    esp_elf_t missing_entry = {0};
    assert(esp_elf_request(&missing_entry, 0, 0, NULL) == -EINVAL);
    assert(!missing_entry.init_done);
    return 0;
}
