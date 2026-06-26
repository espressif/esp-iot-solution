/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "esp_err.h"
#include "test_ramfs_common.h"

TEST_CASE("ramfs functional: basic file io and stat", "[ramfs][functional]")
{
    char buf[32];
    struct stat st;
    const char *text = "hello ramfs";

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, text);
    read_text_file(TEST_RAMFS_FILE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(text, buf);
    TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_FILE, &st));
    TEST_ASSERT_TRUE(S_ISREG(st.st_mode));
    TEST_ASSERT_EQUAL(strlen(text), st.st_size);
    TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_BASE_PATH, &st));
    TEST_ASSERT_TRUE(S_ISDIR(st.st_mode));
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: directory iteration and rename", "[ramfs][functional]")
{
    DIR *dir;
    struct dirent *entry;
    bool found = false;
    struct stat st;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "hello");

    dir = opendir(TEST_RAMFS_DIR);
    TEST_ASSERT_NOT_NULL(dir);
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, "hello.txt") == 0) {
            found = true;
        }
    }
    TEST_ASSERT_EQUAL(0, closedir(dir));
    TEST_ASSERT_TRUE(found);

    TEST_ASSERT_EQUAL(0, rename(TEST_RAMFS_FILE, TEST_RAMFS_RENAMED));
    TEST_ASSERT_EQUAL(-1, stat(TEST_RAMFS_FILE, &st));
    TEST_ASSERT_EQUAL(ENOENT, errno);
    TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_RENAMED, &st));
    TEST_ASSERT_TRUE(S_ISREG(st.st_mode));
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: open flags", "[ramfs][functional]")
{
    int fd;
    char buf[16] = {0};

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));

    fd = open(TEST_RAMFS_FILE, O_CREAT | O_EXCL | O_RDWR, 0666);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(3, write(fd, "abc", 3));
    TEST_ASSERT_EQUAL(0, close(fd));

    errno = 0;
    assert_errno_result(EEXIST, open(TEST_RAMFS_FILE, O_CREAT | O_EXCL | O_RDWR, 0666));

    fd = open(TEST_RAMFS_FILE, O_WRONLY | O_TRUNC);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(1, write(fd, "x", 1));
    TEST_ASSERT_EQUAL(0, close(fd));
    assert_file_text(TEST_RAMFS_FILE, "x");

    fd = open(TEST_RAMFS_FILE, O_WRONLY | O_APPEND);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(0, lseek(fd, 0, SEEK_SET));
    TEST_ASSERT_EQUAL(1, write(fd, "y", 1));
    TEST_ASSERT_EQUAL(0, close(fd));
    assert_file_text(TEST_RAMFS_FILE, "xy");

    fd = open(TEST_RAMFS_FILE, O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(2, read(fd, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("xy", buf);
    errno = 0;
    assert_errno_result(EBADF, write(fd, "z", 1));
    TEST_ASSERT_EQUAL(0, close(fd));
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: lseek sparse write pread and pwrite", "[ramfs][functional]")
{
    int fd;
    uint8_t buf[8] = {0xff};
    const uint8_t expected_sparse[] = {'a', 'b', 'c', 0, 0, 'Z'};
    const uint8_t expected_pwrite[] = {'a', '1', '2', 0, 0, 'Z'};

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));

    fd = open(TEST_RAMFS_FILE, O_CREAT | O_RDWR, 0666);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(3, write(fd, "abc", 3));
    TEST_ASSERT_EQUAL(5, lseek(fd, 5, SEEK_SET));
    TEST_ASSERT_EQUAL(1, write(fd, "Z", 1));
    TEST_ASSERT_EQUAL(6, lseek(fd, 0, SEEK_END));
    TEST_ASSERT_EQUAL(-1, lseek(fd, -7, SEEK_END));
    TEST_ASSERT_EQUAL(EINVAL, errno);
    TEST_ASSERT_EQUAL(6, pread(fd, buf, sizeof(expected_sparse), 0));
    TEST_ASSERT_EQUAL_MEMORY(expected_sparse, buf, sizeof(expected_sparse));
    TEST_ASSERT_EQUAL(2, pwrite(fd, "12", 2, 1));
    TEST_ASSERT_EQUAL(6, lseek(fd, 0, SEEK_CUR));
    TEST_ASSERT_EQUAL(6, pread(fd, buf, sizeof(expected_sparse), 0));
    TEST_ASSERT_EQUAL_MEMORY(expected_pwrite, buf, sizeof(expected_pwrite));
    TEST_ASSERT_EQUAL(0, fsync(fd));
    TEST_ASSERT_EQUAL(0, close(fd));
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: fcntl append toggling", "[ramfs][functional]")
{
    int fd;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "abc");

    fd = open(TEST_RAMFS_FILE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(O_RDWR, fcntl(fd, F_GETFL));
    TEST_ASSERT_EQUAL(0, fcntl(fd, F_SETFL, O_APPEND));
    TEST_ASSERT_EQUAL(O_RDWR | O_APPEND, fcntl(fd, F_GETFL));
    TEST_ASSERT_EQUAL(0, lseek(fd, 0, SEEK_SET));
    TEST_ASSERT_EQUAL(1, write(fd, "d", 1));
    TEST_ASSERT_EQUAL(0, close(fd));
    assert_file_text(TEST_RAMFS_FILE, "abcd");
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: truncate ftruncate and zero extension", "[ramfs][functional]")
{
    int fd;
    uint8_t buf[8] = {0xff};
    const uint8_t expected_extend[] = {'a', 'b', 'c', 0, 0, 0};
    const uint8_t expected_ftruncate[] = {'a', 'b', 0, 0, 0};

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "abcdef");

    TEST_ASSERT_EQUAL(0, truncate(TEST_RAMFS_FILE, 3));
    assert_file_text(TEST_RAMFS_FILE, "abc");
    TEST_ASSERT_EQUAL(0, truncate(TEST_RAMFS_FILE, 6));

    fd = open(TEST_RAMFS_FILE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(6, read(fd, buf, 6));
    TEST_ASSERT_EQUAL_MEMORY(expected_extend, buf, sizeof(expected_extend));
    TEST_ASSERT_EQUAL(0, ftruncate(fd, 2));
    TEST_ASSERT_EQUAL(2, lseek(fd, 0, SEEK_END));
    TEST_ASSERT_EQUAL(0, ftruncate(fd, 5));
    TEST_ASSERT_EQUAL(5, pread(fd, buf, 5, 0));
    TEST_ASSERT_EQUAL_MEMORY(expected_ftruncate, buf, sizeof(expected_ftruncate));
    TEST_ASSERT_EQUAL(0, close(fd));

    errno = 0;
    assert_errno_result(EINVAL, truncate(TEST_RAMFS_FILE, -1));
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: utime and access", "[ramfs][functional]")
{
    struct stat st;
    struct utimbuf times = {
        .actime = 1234,
        .modtime = 5678,
    };

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "time");
    TEST_ASSERT_EQUAL(0, access(TEST_RAMFS_FILE, F_OK));
    TEST_ASSERT_EQUAL(0, utime(TEST_RAMFS_FILE, &times));
    TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_FILE, &st));
    TEST_ASSERT_EQUAL(5678, st.st_mtime);
    errno = 0;
    assert_errno_result(ENOENT, access(TEST_RAMFS_DIR "/missing", F_OK));
    unregister_test_ramfs();
}

TEST_CASE("ramfs functional: multiple independent mounts", "[ramfs][functional]")
{
    size_t total_bytes = 0;
    size_t used_bytes = 0;

    register_test_ramfs_with_limits(TEST_RAMFS_BASE_PATH, 4, 256);
    register_test_ramfs_with_limits(TEST_RAMFS_ALT_BASE_PATH, 4, 512);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_ALT_BASE_PATH "/tmp", 0775));
    write_text_file(TEST_RAMFS_FILE, "one");
    write_text_file(TEST_RAMFS_ALT_BASE_PATH "/tmp/hello.txt", "two");

    assert_file_text(TEST_RAMFS_FILE, "one");
    assert_file_text(TEST_RAMFS_ALT_BASE_PATH "/tmp/hello.txt", "two");
    TEST_ESP_OK(ramfs_info(TEST_RAMFS_ALT_BASE_PATH, &total_bytes, &used_bytes));
    TEST_ASSERT_EQUAL(512, total_bytes);
    TEST_ASSERT_EQUAL(3, used_bytes);
    TEST_ESP_OK(ramfs_unregister(TEST_RAMFS_ALT_BASE_PATH));
    unregister_test_ramfs();
}
