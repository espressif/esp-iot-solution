/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_dlfcn.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_RMMOD";

static struct {
    struct arg_str *file;
    struct arg_end *end;
} rmmod_arg;

static int rmmod_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(rmmod_arg);

    if (dlrmmod(rmmod_arg.file->sval[0])) {
        ESP_LOGW(TAG, "Failed to rmmod: %s", rmmod_arg.file->sval[0]);
        return -1;
    }

    return 0;
}

void shell_regitser_cmd_rmmod(void)
{
    int cmd_num = 1;
    rmmod_arg.file =
        arg_str0(NULL, NULL, "<file>", "File name of the shared object");
    rmmod_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "rmmod",
        .help = "Remove a previously installed module from memory",
        .hint = NULL,
        .func = &rmmod_main,
        .argtable = NULL
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
