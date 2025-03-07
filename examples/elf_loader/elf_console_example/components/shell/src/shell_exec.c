/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "esp_log.h"
#include "esp_dlfcn.h"
#include "esp_elf.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_EXEC";

typedef int (*lib_func1)(void);
typedef int (*lib_func2)(int);

static struct {
    struct arg_str *file;
    struct arg_lit *so;
    struct arg_lit *elf;
    struct arg_end *end;
} exec_arg;

static int dl_sample(const char *filename)
{
    void *handle;
    lib_func1 test_function;
    lib_func2 fibonacci_function;

    handle = dlopen(filename, RTLD_LAZY);
    if (!handle) {
        ESP_LOGE(TAG, "dlopen %s failed!", filename);
        return -1;
    }

    test_function = (lib_func1)dlsym(handle, "try_test");
    if (!test_function) {
        ESP_LOGE(TAG, "dlsym %p failed!", handle);
        dlclose(handle);
        return -1;
    }

    test_function();

    fibonacci_function = (lib_func2)dlsym(handle, "fibonacci");
    if (!fibonacci_function) {
        ESP_LOGE(TAG, "dlsym %p failed!", handle);
        dlclose(handle);
        return -1;
    }

    printf("Fibonacci number at position %d is %d\n", 10, fibonacci_function(10));
    dlclose(handle);

    return 0;
}

static int elf_sample(const char *filename)
{
    int ret;
    elf_file_t file;
    esp_elf_t elf;

    ret = esp_elf_open(&file, filename);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to open file %s", filename);
        return ret;
    }

    printf("Open file:%s, len=%d\n", filename, file.size);

    ret = esp_elf_init(&elf);
    if (ret < 0) {
        esp_elf_close(&file);
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        return ret;
    }

    printf("Start to relocate ELF file\n");

    ret = esp_elf_relocate(&elf, file.payload);
    if (ret < 0) {
        esp_elf_close(&file);
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        return ret;
    }

    esp_elf_request(&elf, 0, 0, NULL);

    printf("Success to exit from ELF file\n");

    esp_elf_deinit(&elf);

    esp_elf_close(&file);

    return 0;
}

static int exec_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(exec_arg);

    if (exec_arg.so->count == 0) {
        return elf_sample(exec_arg.file->sval[0]);
    } else {
        return dl_sample(exec_arg.file->sval[0]);
    }
}

void shell_regitser_cmd_exec(void)
{
    int cmd_num = 2;
    exec_arg.file =
        arg_str0(NULL, NULL, "<file>", "File name of ELF APP or shared object");
    exec_arg.so = arg_lit0("s", "shared object", "Dynamic load shared object");
    exec_arg.elf = arg_lit0("e", "ELF APP", "Execution of ELF applications");
    exec_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "exec",
        .help = "Dynamic load shared object or ELF application from file-system and execute it",
        .hint = NULL,
        .func = &exec_main,
        .argtable = &exec_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
