/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_dlfcn.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_MOD_LOAD";

static struct {
    struct arg_str *file;
    struct arg_end *end;
} mod_load_arg;

static int mod_load_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(mod_load_arg);

    if (mod_load_arg.file->count == 0) {
        ESP_LOGE(TAG, "Missing required argument: <file>");
        return -1;
    }

    void *handle = dlopen(mod_load_arg.file->sval[0], RTLD_LAZY);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to load module: %s, error: %s",
                 mod_load_arg.file->sval[0], dlerror());
        return -1;
    }

    ESP_LOGI(TAG, "Module loaded successfully: %s", mod_load_arg.file->sval[0]);
    return 0;
}

void shell_register_cmd_mod_load(void)
{
    int cmd_num = 1;
    mod_load_arg.file =
        arg_str1(NULL, NULL, "<file>", "File name of the shared object");
    mod_load_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "mod_load",
        .help = "Load the module into memory",
        .hint = NULL,
        .func = &mod_load_main,
        .argtable = &mod_load_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
