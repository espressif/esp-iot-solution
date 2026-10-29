/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"
#include "test_ramfs_common.h"

TEST_CASE("ramfs error: invalid paths and type mismatches", "[ramfs][error]")
{
    int fd;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "type");

    errno = 0;
    assert_errno_result(ENOENT, open(TEST_RAMFS_DIR "/missing.txt", O_RDONLY));
    errno = 0;
    assert_errno_result(EISDIR, open(TEST_RAMFS_DIR, O_RDONLY));
    errno = 0;
    assert_errno_result(EEXIST, mkdir(TEST_RAMFS_DIR, 0775));
    errno = 0;
    assert_errno_result(ENOTDIR, mkdir(TEST_RAMFS_FILE "/child", 0775));
    errno = 0;
    assert_errno_result(EISDIR, unlink(TEST_RAMFS_DIR));
    errno = 0;
    assert_errno_result(ENOTDIR, rmdir(TEST_RAMFS_FILE));

    fd = open(TEST_RAMFS_FILE, O_WRONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    errno = 0;
    TEST_ASSERT_EQUAL(-1, read(fd, &(uint8_t) {
        0
    }, 1));
    TEST_ASSERT_EQUAL(EBADF, errno);
    TEST_ASSERT_EQUAL(0, close(fd));
    unregister_test_ramfs();
}

TEST_CASE("ramfs error: busy unlink and unregister", "[ramfs][error]")
{
    int fd;
    DIR *dir;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "busy");

    fd = open(TEST_RAMFS_FILE, O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    errno = 0;
    assert_errno_result(EBUSY, unlink(TEST_RAMFS_FILE));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ramfs_unregister(TEST_RAMFS_BASE_PATH));
    TEST_ASSERT_EQUAL(0, close(fd));

    dir = opendir(TEST_RAMFS_DIR);
    TEST_ASSERT_NOT_NULL(dir);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ramfs_unregister(TEST_RAMFS_BASE_PATH));
    TEST_ASSERT_EQUAL(0, closedir(dir));
    TEST_ASSERT_EQUAL(0, unlink(TEST_RAMFS_FILE));
    unregister_test_ramfs();
}

TEST_CASE("ramfs error: directory removal rules", "[ramfs][error]")
{
    DIR *dir;

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "child");

    errno = 0;
    assert_errno_result(ENOTEMPTY, rmdir(TEST_RAMFS_DIR));
    TEST_ASSERT_EQUAL(0, unlink(TEST_RAMFS_FILE));

    dir = opendir(TEST_RAMFS_DIR);
    TEST_ASSERT_NOT_NULL(dir);
    errno = 0;
    assert_errno_result(EBUSY, rmdir(TEST_RAMFS_DIR));
    TEST_ASSERT_EQUAL(0, closedir(dir));
    TEST_ASSERT_EQUAL(0, rmdir(TEST_RAMFS_DIR));
    unregister_test_ramfs();
}

TEST_CASE("ramfs error: unsafe rename cases", "[ramfs][error]")
{
    struct stat st;

    register_test_ramfs(2048);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR "/dir_a", 0775));
    write_text_file(TEST_RAMFS_DIR "/a.txt", "aaa");
    write_text_file(TEST_RAMFS_DIR "/b.txt", "bbb");

    TEST_ASSERT_EQUAL(0, rename(TEST_RAMFS_DIR "/a.txt", TEST_RAMFS_DIR "/b.txt"));
    assert_file_text(TEST_RAMFS_DIR "/b.txt", "aaa");
    errno = 0;
    assert_errno_result(EISDIR, rename(TEST_RAMFS_DIR "/b.txt", TEST_RAMFS_DIR "/dir_a"));
    errno = 0;
    assert_errno_result(ENOTDIR, rename(TEST_RAMFS_DIR "/dir_a", TEST_RAMFS_DIR "/b.txt"));
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR "/dir_a/child", 0775));
    errno = 0;
    assert_errno_result(EINVAL, rename(TEST_RAMFS_DIR "/dir_a", TEST_RAMFS_DIR "/dir_a/child/moved"));
    TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_DIR "/dir_a", &st));
    TEST_ASSERT_TRUE(S_ISDIR(st.st_mode));
    unregister_test_ramfs();
}

TEST_CASE("ramfs error: registration arguments and duplicate mounts", "[ramfs][error]")
{
    ramfs_config_t bad_relative = {
        .base_path = "ram",
        .max_files = 1,
        .max_bytes = 1,
        .caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
    };
    ramfs_config_t bad_root = {
        .base_path = "/",
        .max_files = 1,
        .max_bytes = 1,
        .caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
    };
    ramfs_config_t bad_caps = {
        .base_path = TEST_RAMFS_ALT_BASE_PATH,
        .max_files = 1,
        .max_bytes = 1,
        .caps = 0,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ramfs_register(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ramfs_register(&bad_relative));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ramfs_register(&bad_root));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ramfs_register(&bad_caps));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ramfs_register(&(ramfs_config_t) {
        .base_path = "/ram/", .max_files = 1, .max_bytes = 1, .caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    }));
    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ramfs_register(&(ramfs_config_t) {
        .base_path = TEST_RAMFS_BASE_PATH, .max_files = 1, .max_bytes = 1, .caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    }));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ramfs_info(TEST_RAMFS_ALT_BASE_PATH, NULL, NULL));
    unregister_test_ramfs();
}
