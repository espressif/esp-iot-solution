/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_dlfcn.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_LIST";

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
    } else if (list_arg.mod->count > 0) {
        dllist(LIST_MODULE);
    } else {
        ESP_LOGE(TAG, "Missing required argument: <mod> or <sym>, use -h for help");
        return -1;
    }

    return 0;
}

void shell_register_cmd_list(void)
{
    int cmd_num = 2;
    list_arg.mod = arg_lit0("m", "mod", "List loaded shared object modules");
    list_arg.sym = arg_lit0("s", "sym", "List symbols table in shared object");
    list_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "list",
        .help = "List dynamically loaded shared objects",
        .hint = NULL,
        .func = &list_main,
        .argtable = &list_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
