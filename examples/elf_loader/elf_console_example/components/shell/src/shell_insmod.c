/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_dlfcn.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_INSMOD";

static struct {
    struct arg_str *file;
    struct arg_end *end;
} insmod_arg;

static int insmod_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(insmod_arg);

    if (!dlinsmod(insmod_arg.file->sval[0])) {
        ESP_LOGW(TAG, "Failed to insmod: %s", insmod_arg.file->sval[0]);
        return -1;
    }

    return 0;
}

void shell_regitser_cmd_insmod(void)
{
    int cmd_num = 1;
    insmod_arg.file =
        arg_str0(NULL, NULL, "<file>", "File name of the shared object");
    insmod_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "insmod",
        .help = "Load the module into memory",
        .hint = NULL,
        .func = &insmod_main,
        .argtable = &insmod_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
