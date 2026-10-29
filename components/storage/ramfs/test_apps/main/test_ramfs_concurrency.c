/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "test_ramfs_common.h"

typedef struct {
    const char *path;
    char token;
    volatile bool done;
    volatile int result;
} ramfs_task_arg_t;

typedef struct {
    const char *root;
    const char *shared_path;
    uint32_t seed;
    int task_id;
    int iterations;
    volatile bool done;
    volatile int result;
} ramfs_random_task_arg_t;

static uint32_t next_random(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static int write_all(int fd, const void *data, size_t size)
{
    const uint8_t *cur = data;
    size_t left = size;

    while (left > 0) {
        ssize_t written = write(fd, cur, left);

        if (written <= 0) {
            return errno ? errno : EIO;
        }
        cur += written;
        left -= (size_t)written;
    }
    return 0;
}

static int append_token(const char *path, char token)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    int ret;

    if (fd < 0) {
        return errno ? errno : EIO;
    }

    ret = write_all(fd, &token, 1);
    if (close(fd) != 0 && ret == 0) {
        ret = errno ? errno : EIO;
    }
    return ret;
}

static void write_own_file_task(void *arg)
{
    ramfs_task_arg_t *task_arg = arg;
    int fd = open(task_arg->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    task_arg->result = 0;
    if (fd < 0) {
        task_arg->result = errno;
        task_arg->done = true;
        vTaskDelete(NULL);
    }

    for (int i = 0; i < 32; i++) {
        char line[8];
        int len = snprintf(line, sizeof(line), "%c%02d\n", task_arg->token, i);

        if (write(fd, line, len) != len) {
            task_arg->result = errno ? errno : EIO;
            break;
        }
        vTaskDelay(1);
    }

    close(fd);
    task_arg->done = true;
    vTaskDelete(NULL);
}

static void append_shared_file_task(void *arg)
{
    ramfs_task_arg_t *task_arg = arg;
    int fd = open(task_arg->path, O_WRONLY | O_APPEND);

    task_arg->result = 0;
    if (fd < 0) {
        task_arg->result = errno;
        task_arg->done = true;
        vTaskDelete(NULL);
    }

    for (int i = 0; i < 32; i++) {
        if (write(fd, &task_arg->token, 1) != 1) {
            task_arg->result = errno ? errno : EIO;
            break;
        }
        vTaskDelay(1);
    }

    close(fd);
    task_arg->done = true;
    vTaskDelete(NULL);
}

static void random_mixed_task(void *arg)
{
    ramfs_random_task_arg_t *task_arg = arg;
    uint32_t state = task_arg->seed;

    task_arg->result = append_token(task_arg->shared_path, (char)('0' + task_arg->task_id));
    if (task_arg->result != 0) {
        task_arg->done = true;
        vTaskDelete(NULL);
    }

    for (int i = 0; i < task_arg->iterations && task_arg->result == 0; i++) {
        uint32_t value = next_random(&state);
        int slot = (int)(value % 4);
        int op = (int)((value >> 8) % 6);
        char path[96];
        char tmp_path[96];
        char content[32];
        int fd;
        int len;

        snprintf(path, sizeof(path), "%s/t%d_%d.txt", task_arg->root, task_arg->task_id, slot);
        snprintf(tmp_path, sizeof(tmp_path), "%s/t%d_%d.tmp", task_arg->root, task_arg->task_id, slot);
        len = snprintf(content, sizeof(content), "T%d-S%d-%08x\n", task_arg->task_id, slot, (unsigned)value);

        if (op == 0) {
            fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                task_arg->result = errno ? errno : EIO;
            } else {
                task_arg->result = write_all(fd, content, (size_t)len);
                if (close(fd) != 0 && task_arg->result == 0) {
                    task_arg->result = errno ? errno : EIO;
                }
            }
        } else if (op == 1) {
            fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd < 0) {
                task_arg->result = errno ? errno : EIO;
            } else {
                task_arg->result = write_all(fd, content, (size_t)len);
                if (close(fd) != 0 && task_arg->result == 0) {
                    task_arg->result = errno ? errno : EIO;
                }
            }
        } else if (op == 2) {
            char buf[48];

            fd = open(path, O_RDONLY);
            if (fd >= 0) {
                if (read(fd, buf, sizeof(buf)) < 0) {
                    task_arg->result = errno ? errno : EIO;
                }
                if (close(fd) != 0 && task_arg->result == 0) {
                    task_arg->result = errno ? errno : EIO;
                }
            } else if (errno != ENOENT) {
                task_arg->result = errno ? errno : EIO;
            }
        } else if (op == 3) {
            if (rename(path, tmp_path) == 0) {
                if (rename(tmp_path, path) != 0) {
                    task_arg->result = errno ? errno : EIO;
                }
            } else if (errno != ENOENT) {
                task_arg->result = errno ? errno : EIO;
            }
        } else if (op == 4) {
            if (unlink(path) != 0 && errno != ENOENT) {
                task_arg->result = errno ? errno : EIO;
            }
        } else {
            task_arg->result = append_token(task_arg->shared_path, (char)('A' + task_arg->task_id));
        }

        vTaskDelay((TickType_t)(value & 0x03));
    }

    if (task_arg->result == 0) {
        task_arg->result = append_token(task_arg->shared_path, (char)('a' + task_arg->task_id));
    }
    task_arg->done = true;
    vTaskDelete(NULL);
}

static void wait_task_done(ramfs_task_arg_t *arg)
{
    for (int i = 0; i < 200 && !arg->done; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    TEST_ASSERT_TRUE(arg->done);
    TEST_ASSERT_EQUAL(0, arg->result);
}

static void wait_random_task_done(ramfs_random_task_arg_t *arg)
{
    for (int i = 0; i < 400 && !arg->done; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    TEST_ASSERT_TRUE(arg->done);
    TEST_ASSERT_EQUAL(0, arg->result);
}

TEST_CASE("ramfs concurrency: parallel writes to independent files", "[ramfs][concurrency]")
{
    ramfs_task_arg_t arg_a = {
        .path = TEST_RAMFS_DIR "/task_a.txt",
        .token = 'A',
    };
    ramfs_task_arg_t arg_b = {
        .path = TEST_RAMFS_DIR "/task_b.txt",
        .token = 'B',
    };
    struct stat st;

    register_test_ramfs(4096);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(write_own_file_task, "ramfs_a", 3072, &arg_a, 5, NULL));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(write_own_file_task, "ramfs_b", 3072, &arg_b, 5, NULL));
    wait_task_done(&arg_a);
    wait_task_done(&arg_b);
    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL(0, stat(arg_a.path, &st));
    TEST_ASSERT_GREATER_THAN(0, st.st_size);
    TEST_ASSERT_EQUAL(0, stat(arg_b.path, &st));
    TEST_ASSERT_GREATER_THAN(0, st.st_size);
    unregister_test_ramfs();
}

TEST_CASE("ramfs concurrency: parallel append to one file", "[ramfs][concurrency]")
{
    ramfs_task_arg_t arg_a = {
        .path = TEST_RAMFS_FILE,
        .token = 'A',
    };
    ramfs_task_arg_t arg_b = {
        .path = TEST_RAMFS_FILE,
        .token = 'B',
    };
    int fd;
    char buf[80];
    ssize_t read_len;
    int count_a = 0;
    int count_b = 0;

    register_test_ramfs(4096);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_FILE, "");
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(append_shared_file_task, "ramfs_append_a", 3072, &arg_a, 5, NULL));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(append_shared_file_task, "ramfs_append_b", 3072, &arg_b, 5, NULL));
    wait_task_done(&arg_a);
    wait_task_done(&arg_b);
    vTaskDelay(pdMS_TO_TICKS(50));

    fd = open(TEST_RAMFS_FILE, O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    read_len = read(fd, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(64, read_len);
    TEST_ASSERT_EQUAL(0, close(fd));
    for (int i = 0; i < read_len; i++) {
        count_a += buf[i] == 'A';
        count_b += buf[i] == 'B';
    }
    TEST_ASSERT_EQUAL(32, count_a);
    TEST_ASSERT_EQUAL(32, count_b);
    unregister_test_ramfs();
}

TEST_CASE("ramfs concurrency: deterministic random mixed operations", "[ramfs][concurrency]")
{
    ramfs_random_task_arg_t args[] = {
        {
            .root = TEST_RAMFS_DIR,
            .shared_path = TEST_RAMFS_DIR "/random_shared.log",
            .seed = 0x12345678,
            .task_id = 0,
            .iterations = 640,
        },
        {
            .root = TEST_RAMFS_DIR,
            .shared_path = TEST_RAMFS_DIR "/random_shared.log",
            .seed = 0x87654321,
            .task_id = 1,
            .iterations = 643,
        },
        {
            .root = TEST_RAMFS_DIR,
            .shared_path = TEST_RAMFS_DIR "/random_shared.log",
            .seed = 0x0badcafe,
            .task_id = 2,
            .iterations = 642,
        },
        {
            .root = TEST_RAMFS_DIR,
            .shared_path = TEST_RAMFS_DIR "/random_shared.log",
            .seed = 0xfeedbeef,
            .task_id = 3,
            .iterations = 641,
        },
    };
    struct stat st;

    register_test_ramfs(12 * 1024);
    TEST_ASSERT_EQUAL(0, mkdir(TEST_RAMFS_DIR, 0775));
    write_text_file(TEST_RAMFS_DIR "/random_shared.log", "");

    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
        char name[16];

        snprintf(name, sizeof(name), "ramfs_mix_%u", (unsigned)i);
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(random_mixed_task, name, 4096, &args[i], 5, NULL));
    }
    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
        wait_random_task_done(&args[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL(0, stat(TEST_RAMFS_DIR "/random_shared.log", &st));
    TEST_ASSERT_GREATER_OR_EQUAL(sizeof(args) / sizeof(args[0]) * 2, st.st_size);
    unregister_test_ramfs();
}
