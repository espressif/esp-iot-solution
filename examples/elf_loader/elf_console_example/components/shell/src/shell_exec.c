/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "esp_log.h"
#include "esp_elf.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_EXEC";

static struct {
    struct arg_str *file;
    struct arg_end *end;
} exec_arg;

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
        esp_elf_deinit(&elf);
        esp_elf_close(&file);
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        return ret;
    }

    ret = esp_elf_request(&elf, 0, 0, NULL);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to execute ELF file errno=%d", ret);
        esp_elf_deinit(&elf);
        esp_elf_close(&file);
        return ret;
    }

    printf("Success to exit from ELF file\n");

    esp_elf_deinit(&elf);

    esp_elf_close(&file);

    return 0;
}

static int exec_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(exec_arg);

    if (exec_arg.file->count == 0) {
        ESP_LOGE(TAG, "File path is required");
        return -1;
    }

    return elf_sample(exec_arg.file->sval[0]);
}

void shell_register_cmd_exec(void)
{
    int cmd_num = 1;
    exec_arg.file =
        arg_str1(NULL, NULL, "<file>", "File name of ELF APP");
    exec_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "exec",
        .help = "Dynamic load ELF application from file-system and execute it",
        .hint = NULL,
        .func = &exec_main,
        .argtable = &exec_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
