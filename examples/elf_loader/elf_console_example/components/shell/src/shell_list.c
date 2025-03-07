/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_dlfcn.h"

#include "shell_cmd.h"

static struct {
    struct arg_lit *mod;
    struct arg_lit *sym;
    struct arg_end *end;
} list_arg;

static int list_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(list_arg);

    if (list_arg.sym->count > 0) {
        dllist(LIST_SYMBOL);
    } else {
        dllist(LIST_MODULE);
    }

    return 0;
}

void shell_regitser_cmd_list(void)
{
    int cmd_num = 2;
    list_arg.mod = arg_lit0("m", "shared modules", "Dynamic load a list of shared object modules");
    list_arg.sym = arg_lit0("s", "symbols table", "Symbols table in shared object");
    list_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "list",
        .help = "List dynamic load shared objects",
        .hint = NULL,
        .func = &list_main,
        .argtable = &list_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
