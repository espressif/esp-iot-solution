/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "test_ramfs_common.h"

#define TEST_FATFS_SYNC_DIR  "/fatfs/ramfs_sync"
#define TEST_FATFS_FILE      TEST_FATFS_SYNC_DIR "/hello.txt"
#define TEST_FATFS_TREE_DIR  TEST_FATFS_SYNC_DIR "/tree"
#define TEST_FATFS_LOAD_DIR  TEST_FATFS_SYNC_DIR "/load_src"

static esp_err_t run_timed_sync_op(const char *name, esp_err_t (*op)(const char *, const char *), const char *src_path, const char *dst_path)
{
    int64_t start_us = esp_timer_get_time();
    esp_err_t err = op(src_path, dst_path);
    int64_t elapsed_us = esp_timer_get_time() - start_us;

    printf("ramfs sync timing: %s took %lld.%03lld ms\n", name, elapsed_us / 1000, elapsed_us % 1000);
    return err;
}

static void remove_tree_if_exists(const char *path)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;

    if (stat(path, &st) != 0) {
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        unlink(path);
        return;
    }

    dir = opendir(path);
    if (!dir) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        char *child;
        size_t child_len;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        child_len = strlen(path) + 1 + strlen(entry->d_name) + 1;
        child = malloc(child_len);
        if (!child) {
            continue;
        }
        snprintf(child, child_len, "%s/%s", path, entry->d_name);
        remove_tree_if_exists(child);
        free(child);
    }
    closedir(dir);
    rmdir(path);
}

static void ensure_dir_exists(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return;
        }
        TEST_FAIL_MESSAGE("path exists but is not a directory");
    }
    if (mkdir(path, 0775) != 0 && errno != EEXIST) {
        char msg[128];

        snprintf(msg, sizeof(msg), "mkdir failed: path=%s errno=%d", path, errno);
        TEST_FAIL_MESSAGE(msg);
    }
}

static void ensure_parent_dirs_exist(const char *path)
{
    char *copy = strdup(path);
    char *slash;

    TEST_ASSERT_NOT_NULL(copy);
    slash = strrchr(copy, '/');
    if (!slash || slash == copy) {
        free(copy);
        return;
    }

    *slash = '\0';
    for (char *p = copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            ensure_dir_exists(copy);
            *p = '/';
        }
    }
    ensure_dir_exists(copy);
    free(copy);
}

static void prepare_sync_dir(void)
{
    remove_tree_if_exists(TEST_FATFS_SYNC_DIR);
    ensure_parent_dirs_exist(TEST_FATFS_SYNC_DIR "/.keep");
}

TEST_CASE("ramfs sync: file to fatfs", "[ramfs][sync]")
{
    register_test_ramfs(2048);
    prepare_sync_dir();
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "persist me");

    TEST_ESP_OK(run_timed_sync_op("file to fatfs", ramfs_sync_file_to_fatfs, TEST_RAMFS_FILE, TEST_FATFS_FILE));
    assert_file_text(TEST_FATFS_FILE, "persist me");
    unregister_test_ramfs();
}

TEST_CASE("ramfs sync: tree to fatfs", "[ramfs][sync]")
{
    register_test_ramfs(4096);
    prepare_sync_dir();
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR "/sub", 0775));
    write_text_file(TEST_RAMFS_DIR "/root.txt", "root");
    write_text_file(TEST_RAMFS_DIR "/sub/child.txt", "child");

    TEST_ESP_OK(run_timed_sync_op("tree to fatfs", ramfs_sync_tree_to_fatfs, TEST_RAMFS_DIR, TEST_FATFS_TREE_DIR));
    assert_file_text(TEST_FATFS_TREE_DIR "/root.txt", "root");
    assert_file_text(TEST_FATFS_TREE_DIR "/sub/child.txt", "child");
    unregister_test_ramfs();
}

TEST_CASE("ramfs sync: load file from fatfs", "[ramfs][sync]")
{
    register_test_ramfs(2048);
    prepare_sync_dir();
    write_text_file(TEST_FATFS_FILE, "loaded");
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));

    TEST_ESP_OK(run_timed_sync_op("load file from fatfs", ramfs_load_file_from_fatfs, TEST_FATFS_FILE, TEST_RAMFS_FILE));
    assert_file_text(TEST_RAMFS_FILE, "loaded");
    unregister_test_ramfs();
}

TEST_CASE("ramfs sync: load tree from fatfs", "[ramfs][sync]")
{
    register_test_ramfs(4096);
    prepare_sync_dir();
    ensure_dir_exists(TEST_FATFS_LOAD_DIR);
    ensure_dir_exists(TEST_FATFS_LOAD_DIR "/sub");
    write_text_file(TEST_FATFS_LOAD_DIR "/a.txt", "aaa");
    write_text_file(TEST_FATFS_LOAD_DIR "/sub/b.txt", "bbb");

    TEST_ESP_OK(run_timed_sync_op("load tree from fatfs", ramfs_load_tree_from_fatfs, TEST_FATFS_LOAD_DIR, TEST_RAMFS_DIR));
    assert_file_text(TEST_RAMFS_DIR "/a.txt", "aaa");
    assert_file_text(TEST_RAMFS_DIR "/sub/b.txt", "bbb");
    unregister_test_ramfs();
}

TEST_CASE("ramfs sync: busy ramfs nodes are rejected", "[ramfs][sync]")
{
    int fd;

    register_test_ramfs(2048);
    prepare_sync_dir();
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "busy");
    write_text_file(TEST_FATFS_FILE, "new");

    fd = open(TEST_RAMFS_FILE, O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ramfs_sync_file_to_fatfs(TEST_RAMFS_FILE, TEST_FATFS_FILE));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ramfs_load_file_from_fatfs(TEST_FATFS_FILE, TEST_RAMFS_FILE));
    TEST_ASSERT_EQUAL(0, close(fd));
    unregister_test_ramfs();
}
