/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test_ramfs_common.h"

TEST_CASE("ramfs stability: repeated mount and unmount", "[ramfs][stability]")
{
    for (int i = 0; i < 16; i++) {
        register_test_ramfs(512);
        TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
        write_text_file(TEST_RAMFS_FILE, "cycle");
        assert_file_text(TEST_RAMFS_FILE, "cycle");
        unregister_test_ramfs();
    }
}

TEST_CASE("ramfs stability: repeated create read unlink cycles", "[ramfs][stability]")
{
    char path[64];
    char text[16];

    register_test_ramfs(4096);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    for (int i = 0; i < 32; i++) {
        snprintf(path, sizeof(path), TEST_RAMFS_DIR "/f_%02d.txt", i);
        snprintf(text, sizeof(text), "v_%02d", i);
        write_text_file(path, text);
        assert_file_text(path, text);
        TEST_ASSERT_EQUAL(0, unlink(path));
    }
    unregister_test_ramfs();
}

TEST_CASE("ramfs stability: repeated overwrite same file", "[ramfs][stability]")
{
    char text[24];

    register_test_ramfs(1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    for (int i = 0; i < 64; i++) {
        snprintf(text, sizeof(text), "overwrite_%02d", i);
        write_text_file(TEST_RAMFS_FILE, text);
        assert_file_text(TEST_RAMFS_FILE, text);
    }
    unregister_test_ramfs();
}
