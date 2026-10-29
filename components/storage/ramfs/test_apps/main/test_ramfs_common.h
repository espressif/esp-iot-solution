/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "ramfs.h"
#include "unity.h"

#define TEST_RAMFS_BASE_PATH     "/ram"
#define TEST_RAMFS_ALT_BASE_PATH "/ram2"
#define TEST_RAMFS_DIR           TEST_RAMFS_BASE_PATH "/tmp"
#define TEST_RAMFS_FILE          TEST_RAMFS_DIR "/hello.txt"
#define TEST_RAMFS_RENAMED       TEST_RAMFS_DIR "/renamed.txt"
#define TEST_RAMFS_BIG           TEST_RAMFS_DIR "/big.bin"

static inline void register_test_ramfs_with_limits(const char *base_path, size_t max_files, size_t max_bytes)
{
    ramfs_config_t config = {
        .base_path = base_path,
        .max_files = max_files,
        .max_bytes = max_bytes,
        .caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
    };

    TEST_ESP_OK(ramfs_register(&config));
}

static inline void register_test_ramfs(size_t max_bytes)
{
    register_test_ramfs_with_limits(TEST_RAMFS_BASE_PATH, 8, max_bytes);
}

static inline void unregister_test_ramfs(void)
{
    TEST_ESP_OK(ramfs_unregister(TEST_RAMFS_BASE_PATH));
}

static inline void write_text_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");

    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL(0, fclose(file));
}

static inline void read_text_file(const char *path, char *buf, size_t size)
{
    FILE *file = fopen(path, "rb");
    size_t len;

    TEST_ASSERT_NOT_NULL(file);
    len = fread(buf, 1, size - 1, file);
    TEST_ASSERT_FALSE(ferror(file));
    buf[len] = '\0';
    TEST_ASSERT_EQUAL(0, fclose(file));
}

static inline void assert_file_text(const char *path, const char *expected)
{
    char buf[96];

    read_text_file(path, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static inline void assert_errno_result(int expected_errno, int result)
{
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_EQUAL(expected_errno, errno);
}
