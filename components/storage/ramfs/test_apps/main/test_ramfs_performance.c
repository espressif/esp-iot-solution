/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "esp_timer.h"
#include "test_ramfs_common.h"

TEST_CASE("ramfs performance: sequential write and read", "[ramfs][performance]")
{
    int fd;
    uint8_t buf[256];
    int64_t start_us;
    int64_t elapsed_us;
    size_t total = 0;

    register_test_ramfs(16 * 1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    memset(buf, 0x3c, sizeof(buf));

    fd = open(TEST_RAMFS_BIG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    start_us = esp_timer_get_time();
    for (int i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL(sizeof(buf), write(fd, buf, sizeof(buf)));
        total += sizeof(buf);
    }
    elapsed_us = esp_timer_get_time() - start_us;
    TEST_ASSERT_EQUAL(0, close(fd));
    TEST_ASSERT_EQUAL(8 * 1024, total);
    TEST_ASSERT_GREATER_THAN(0, elapsed_us);
    TEST_ASSERT_LESS_THAN(1000000, elapsed_us);

    fd = open(TEST_RAMFS_BIG, O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    total = 0;
    start_us = esp_timer_get_time();
    while (read(fd, buf, sizeof(buf)) == sizeof(buf)) {
        total += sizeof(buf);
    }
    elapsed_us = esp_timer_get_time() - start_us;
    TEST_ASSERT_EQUAL(0, close(fd));
    TEST_ASSERT_EQUAL(8 * 1024, total);
    TEST_ASSERT_GREATER_THAN(0, elapsed_us);
    TEST_ASSERT_LESS_THAN(1000000, elapsed_us);
    unregister_test_ramfs();
}

TEST_CASE("ramfs performance: repeated stat and open close", "[ramfs][performance]")
{
    struct stat st;
    int64_t start_us;
    int64_t elapsed_us;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "perf");

    start_us = esp_timer_get_time();
    for (int i = 0; i < 64; i++) {
        int fd;

        TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_FILE, &st));
        fd = open(TEST_RAMFS_FILE, O_RDONLY);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
        TEST_ASSERT_EQUAL(0, close(fd));
    }
    elapsed_us = esp_timer_get_time() - start_us;
    TEST_ASSERT_GREATER_THAN(0, elapsed_us);
    TEST_ASSERT_LESS_THAN(1000000, elapsed_us);
    unregister_test_ramfs();
}
