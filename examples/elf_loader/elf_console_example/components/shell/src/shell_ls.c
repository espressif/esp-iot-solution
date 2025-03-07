/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/errno.h>

#include "esp_log.h"

#include "shell_cmd.h"

#ifdef CONFIG_ELF_FILE_SYSTEM_BASE_PATH
#define SHELL_ROOT_FS_PATH  CONFIG_ELF_FILE_SYSTEM_BASE_PATH
#else
#define SHELL_ROOT_FS_PATH "/storage"
#endif

static const char TAG[] = "shell_dir";

static struct {
    struct arg_str *dir;
    struct arg_end *end;
} ls_main_arg;

static int ls_main(int argc, char **argv)
{
    int ret;
    DIR *dir;
    struct dirent *de;
    char *file_path;
    const char *pwd;

    SHELL_CMD_CHECK(ls_main_arg);

    if (ls_main_arg.dir->count) {
        pwd = ls_main_arg.dir->sval[0];
    } else {
        pwd = getenv("$PWD");
        if (!pwd) {
            ESP_LOGE(TAG, "failed to getenv errno=%d", errno);
            return -1;
        }
    }

    ret = asprintf(&file_path, SHELL_ROOT_FS_PATH"/%s", pwd);
    if (ret < 0) {
        ESP_LOGE(TAG, "failed to asprintf errno=%d", errno);
        return -1;
    }

    dir = opendir(file_path);
    if (!dir) {
        ESP_LOGE(TAG, "failed to opendir %s errno=%d", file_path, errno);
        free(file_path);
        return -1;
    }

    do {
        de = readdir(dir);
        if (!de) {
            break;
        }

        if (de->d_type == DT_DIR) {
            printf("\033[0;34m%s\033[0m\n", de->d_name);
        } else {
            printf("%s\n", de->d_name);
        }
    } while (1);

    closedir(dir);
    free(file_path);

    return 0;
}

void shell_regitser_cmd_ls(void)
{
    ls_main_arg.dir = arg_str0(NULL, NULL, "<file_or_directory>", "list directory contents");
    ls_main_arg.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "ls",
        .help = "List information about the FILEs (the current directory by default)",
        .hint = NULL,
        .func = &ls_main,
        .argtable = &ls_main_arg
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
