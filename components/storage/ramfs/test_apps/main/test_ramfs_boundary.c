/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "test_ramfs_common.h"

TEST_CASE("ramfs boundary: capacity limit and usage report", "[ramfs][boundary]")
{
    int fd;
    uint8_t chunk[128];
    size_t total = 0;
    size_t total_bytes = 0;
    size_t used_bytes = 0;
    ssize_t written;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    memset(chunk, 0xa5, sizeof(chunk));

    fd = open(TEST_RAMFS_BIG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    while ((written = write(fd, chunk, sizeof(chunk))) == (ssize_t)sizeof(chunk)) {
        total += (size_t)written;
    }
    TEST_ASSERT_GREATER_THAN(0, total);
    TEST_ASSERT_EQUAL(-1, written);
    TEST_ASSERT_EQUAL(ENOSPC, errno);
    TEST_ASSERT_EQUAL(0, close(fd));
    TEST_ESP_OK(ramfs_info(TEST_RAMFS_BASE_PATH, &total_bytes, &used_bytes));
    TEST_ASSERT_EQUAL(1024, total_bytes);
    TEST_ASSERT_LESS_OR_EQUAL(total_bytes, used_bytes);
    unregister_test_ramfs();
}

TEST_CASE("ramfs boundary: exact capacity write succeeds", "[ramfs][boundary]")
{
    int fd;
    uint8_t data[256];
    size_t total_bytes = 0;
    size_t used_bytes = 0;

    register_test_ramfs(256);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    memset(data, 0x5a, sizeof(data));

    fd = open(TEST_RAMFS_BIG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(sizeof(data), write(fd, data, sizeof(data)));
    TEST_ASSERT_EQUAL(0, close(fd));
    TEST_ESP_OK(ramfs_info(TEST_RAMFS_BASE_PATH, &total_bytes, &used_bytes));
    TEST_ASSERT_EQUAL(256, total_bytes);
    TEST_ASSERT_EQUAL(256, used_bytes);
    unregister_test_ramfs();
}

TEST_CASE("ramfs boundary: max open files", "[ramfs][boundary]")
{
    int fd0;
    int fd1;

    register_test_ramfs_with_limits(TEST_RAMFS_BASE_PATH, 2, 1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_DIR "/a.txt", "a");
    write_text_file(TEST_RAMFS_DIR "/b.txt", "b");

    fd0 = open(TEST_RAMFS_DIR "/a.txt", O_RDONLY);
    fd1 = open(TEST_RAMFS_DIR "/b.txt", O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd0);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd1);
    errno = 0;
    assert_errno_result(ENFILE, open(TEST_RAMFS_FILE, O_CREAT | O_RDONLY, 0666));
    TEST_ASSERT_EQUAL(0, close(fd0));
    TEST_ASSERT_EQUAL(0, close(fd1));
    unregister_test_ramfs();
}

TEST_CASE("ramfs boundary: zero byte files and zero byte operations", "[ramfs][boundary]")
{
    int fd;
    char byte = 0x7f;
    size_t total_bytes = 0;
    size_t used_bytes = 0;

    register_test_ramfs(128);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    fd = open(TEST_RAMFS_FILE, O_CREAT | O_RDWR, 0666);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(0, write(fd, "", 0));
    TEST_ASSERT_EQUAL(0, read(fd, &byte, 0));
    TEST_ASSERT_EQUAL(0, close(fd));
    TEST_ESP_OK(ramfs_info(TEST_RAMFS_BASE_PATH, &total_bytes, &used_bytes));
    TEST_ASSERT_EQUAL(128, total_bytes);
    TEST_ASSERT_EQUAL(0, used_bytes);
    unregister_test_ramfs();
}
