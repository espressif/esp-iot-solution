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
static size_t free_count;
static esp_elf_t *active_elf;

enum {
    TEXT_BYTES = 32,
    DATA_BYTES = 16,
};

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
    if (ptr) {
        ++free_count;
    }
    free(ptr);
}

static size_t allocate_image(esp_elf_t *elf)
{
#ifdef CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    elf->ptext = malloc(TEXT_BYTES);
    elf->pdata = malloc(DATA_BYTES);
    assert(elf->ptext && elf->pdata);
    return 2;
#else
    elf->psegment = malloc(TEXT_BYTES + DATA_BYTES);
    assert(elf->psegment);
    return 1;
#endif
}

static void check_order(bool legacy, bool explicit_finalize)
{
    void (*pre[])(void) = {pre1, pre2};
    void (*init[])(void) = {init1, init2};
    void (*fini[])(void) = {NULL, fini1, (void (*)(void))UINTPTR_MAX, fini2};
    esp_elf_t elf = {
        .entry = entry,
        .preinit_array = pre, .preinit_count = 2,
        .init_array = init, .init_count = 2, .init_reverse = legacy,
        .fini_array = fini, .fini_count = 4, .fini_reverse = !legacy,
    };
    const size_t previous_frees = free_count;
    const size_t allocations = allocate_image(&elf);
    memset(trace, 0, sizeof(trace));
    trace_pos = 0;
    assert(esp_elf_request(&elf, 0, 0, NULL) == 0);
    assert(strcmp(trace, legacy ? "PQBAE" : "PQABE") == 0);
    assert(esp_elf_request(&elf, 0, 0, NULL) == 0);
    assert(strcmp(trace, legacy ? "PQBAEE" : "PQABEE") == 0);
    if (explicit_finalize) {
        assert(esp_elf_finalize(&elf) == 0);
        assert(strcmp(trace, legacy ? "PQBAEEXY" : "PQABEEYX") == 0);
        assert(free_count == previous_frees);
        assert(elf.entry == entry);
        assert(esp_elf_finalize(&elf) == 0);
        assert(esp_elf_request(&elf, 0, 0, NULL) == -EINVAL);
    }
    esp_elf_deinit(&elf);
    assert(strcmp(trace, legacy ? "PQBAEEXY" : "PQABEEYX") == 0);
    assert(free_count == previous_frees + allocations);
    assert(!elf.init_done);
    assert(!elf.entry);
    esp_elf_deinit(&elf);
    assert(strcmp(trace, legacy ? "PQBAEEXY" : "PQABEEYX") == 0);
}

static void check_finalize_without_execution(void)
{
    void (*fini[])(void) = {fini1};
    esp_elf_t elf = {.entry = entry, .fini_array = fini, .fini_count = 1};
    trace_pos = 0;
    trace[0] = '\0';
    assert(esp_elf_finalize(NULL) == -EINVAL);
    assert(esp_elf_finalize(&elf) == 0);
    assert(esp_elf_finalize(&elf) == 0);
    assert(esp_elf_request(&elf, 0, 0, NULL) == -EINVAL);
    esp_elf_deinit(&elf);
    assert(trace_pos == 0);

    /* A released object can host a new image, including one without fini hooks. */
    elf.entry = entry;
    assert(esp_elf_request(&elf, 0, 0, NULL) == 0);
    assert(esp_elf_finalize(&elf) == 0);
    assert(strcmp(trace, "E") == 0);
    esp_elf_deinit(&elf);
}

static void reject_reentrant_lifecycle(void)
{
    const size_t previous_frees = free_count;
    assert(esp_elf_finalize(active_elf) == -EBUSY);
    assert(esp_elf_request(active_elf, 0, 0, NULL) == -EBUSY);
    esp_elf_deinit(active_elf);
    assert(active_elf->entry != NULL);
    assert(free_count == previous_frees);
    record('R');
}

static int reentrant_entry(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    reject_reentrant_lifecycle();
    return 0;
}

static void check_reentrant_lifecycle_retains_image(void)
{
    void (*callbacks[])(void) = {reject_reentrant_lifecycle};
    esp_elf_t elf = {
        .entry = reentrant_entry,
        .init_array = callbacks, .init_count = 1,
        .fini_array = callbacks, .fini_count = 1,
    };
    active_elf = &elf;
    trace_pos = 0;
    trace[0] = '\0';
    const size_t previous_frees = free_count;
    const size_t allocations = allocate_image(&elf);
    assert(esp_elf_request(&elf, 0, 0, NULL) == 0);
    assert(strcmp(trace, "RR") == 0);
    assert(esp_elf_finalize(&elf) == 0);
    assert(strcmp(trace, "RRR") == 0);
    esp_elf_deinit(&elf);
    assert(free_count == previous_frees + allocations);
    active_elf = NULL;
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
    check_order(false, false);  /* .init_array ascending; .fini_array descending */
    check_order(true, false);   /* legacy .ctors descending; .dtors ascending */
    check_order(false, true);
    check_order(true, true);
    check_finalize_without_execution();
    check_reentrant_lifecycle_retains_image();
    check_section_mapping();
    esp_elf_t missing_entry = {0};
    assert(esp_elf_request(&missing_entry, 0, 0, NULL) == -EINVAL);
    assert(!missing_entry.init_done);
    return 0;
}
