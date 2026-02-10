/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_dlfcn.h"
#include "private/esp_dlmod.h"

#include "shell_cmd.h"

static const char TAG[] = "SHELL_MOD_UNLOAD";

static struct {
    struct arg_str *file;
    struct arg_end *end;
} mod_unload_arg;

static int mod_unload(const char *path)
{
    if (!path) {
        ESP_LOGE(TAG, "Invalid NULL path");
        return -1;
    }

    char filename[FILE_NAME_MAX] = {0};
    if (!dlmod_getname(path, filename, FILE_NAME_MAX)) {
        ESP_LOGE(TAG, "Failed to extract filename from path: %s", path);
        return -1;
    }

    void *handle = dlmod_gethandle(filename);
    if (!handle) {
        ESP_LOGE(TAG, "Module not found: %s", filename);
        return -1;
    }

    int ret = dlmod_remove(handle);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to remove module: %s", filename);
        return -1;
    }

    return 0;
}

static int mod_unload_main(int argc, char **argv)
{
    SHELL_CMD_CHECK(mod_unload_arg);

    if (mod_unload_arg.file->count == 0) {
        ESP_LOGE(TAG, "Missing required file argument");
        return -1;
    }

    if (mod_unload(mod_unload_arg.file->sval[0]) != 0) {
        ESP_LOGW(TAG, "Failed to unload module: %s", mod_unload_arg.file->sval[0]);
        return -1;
    }

    return 0;
}

void shell_register_cmd_mod_unload(void)
{
    int cmd_num = 1;
    mod_unload_arg.file =
        arg_str1(NULL, NULL, "<file>", "File name of the shared object");
    mod_unload_arg.end = arg_end(cmd_num);

    const esp_console_cmd_t cmd = {
        .command = "mod_unload",
        .help = "Unload the module from memory",
        .hint = NULL,
        .func = &mod_unload_main,
        .argtable = &mod_unload_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
