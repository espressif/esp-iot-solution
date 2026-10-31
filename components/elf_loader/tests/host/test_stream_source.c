/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/esp_elf.c"

typedef struct test_file_source {
    FILE *file;
    size_t size;
    unsigned close_count;
    bool fail_reads;
} test_file_source_t;

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
    if (strcmp(name, "printf") == 0) {
        return 0x10000000U;
    }
    if (strcmp(name, "sleep") == 0) {
        return 0x10000004U;
    }
    return 0;
}

static size_t test_file_size(void *ctx)
{
    const test_file_source_t *source = ctx;
    return source->size;
}

static int test_file_read(void *ctx, size_t offset, void *buf, size_t len)
{
    test_file_source_t *source = ctx;
    if (source->fail_reads) {
        return -EIO;
    }
    if (offset > LONG_MAX) {
        return -EOVERFLOW;
    }
    if (fseek(source->file, (long)offset, SEEK_SET) != 0) {
        return -EIO;
    }
    return fread(buf, 1, len, source->file) == len ? 0 : -EIO;
}

static void test_file_close(void *ctx)
{
    test_file_source_t *source = ctx;
    ++source->close_count;
    if (source->file) {
        fclose(source->file);
        source->file = NULL;
    }
}

static test_file_source_t open_source(const char *path)
{
    test_file_source_t source = {0};
    source.file = fopen(path, "rb");
    assert(source.file);
    assert(fseek(source.file, 0, SEEK_END) == 0);
    long size = ftell(source.file);
    assert(size > 0 && size <= INT32_MAX);
    source.size = (size_t)size;
    rewind(source.file);
    return source;
}

static elf_source_t make_source(test_file_source_t *source)
{
    return (elf_source_t) {
        .size = test_file_size,
        .read = test_file_read,
        .close = test_file_close,
        .ctx = source,
    };
}

int main(int argc, char **argv)
{
    assert(argc == 2);

    test_file_source_t source = open_source(argv[1]);
    elf_source_t src = make_source(&source);
    esp_elf_t elf;
    assert(esp_elf_init(&elf) == 0);
    assert(esp_elf_relocate_source(&elf, &src) == 0);
    assert(source.close_count == 1);
    assert(source.file == NULL);
    assert(elf.entry != NULL);
    esp_elf_deinit(&elf);

    test_file_source_t failing = open_source(argv[1]);
    failing.fail_reads = true;
    src = make_source(&failing);
    assert(esp_elf_init(&elf) == 0);
    assert(esp_elf_relocate_source(&elf, &src) < 0);
    assert(failing.close_count == 1);
    assert(failing.file == NULL);
    esp_elf_deinit(&elf);

    test_file_source_t no_elf = open_source(argv[1]);
    src = make_source(&no_elf);
    assert(esp_elf_relocate_source(NULL, &src) == -EINVAL);
    assert(no_elf.close_count == 1);
    assert(no_elf.file == NULL);

    return 0;
}
