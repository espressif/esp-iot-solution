/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>

#include "esp_log.h"
#include "esp_elf.h"

static const char *TAG = "elf_loader";

#if CONFIG_IDF_TARGET_ARCH_XTENSA
extern const uint8_t test_elf_start[] asm("_binary_test_xtensa_elf_start");
extern const uint8_t test_elf_end[]   asm("_binary_test_xtensa_elf_end");
#elif CONFIG_IDF_TARGET_ARCH_RISCV
extern const uint8_t test_elf_start[] asm("_binary_test_riscv_elf_start");
extern const uint8_t test_elf_end[]   asm("_binary_test_riscv_elf_end");
#endif

#ifdef CONFIG_ELF_LOADER_MAIN_ARGS
static int elf_args_decode(const char *str, int *argc, char ***argv)
{
    int n;
    char **argv_buf;
    char *s;
    char *pbuf;
    int len;

    s = (char *)str;
    n = 1;
    len = 1;
    while (*s) {
        if (*s++ == ' ') {
            n++;
        }

        len++;
    }

    pbuf = malloc(n * sizeof(char *) + len);
    if (!pbuf) {
        return -ENOMEM;
    }

    argv_buf = (char **)pbuf;
    s = pbuf + n * sizeof(char *);
    memcpy(s, str, len);
    for (int i = 0; i < n; i++) {
        argv_buf[i] = s;

        while (*s != ' ' || *s == 0) {
            s++;
        }

        *s++ = 0;
    }

    *argc = n;
    *argv = argv_buf;

    return 0;
}
#endif

int app_main(void)
{
    int ret;
    int argc;
    char **argv;
    esp_elf_t elf;
    const uint8_t *buffer = test_elf_start;

    ESP_LOGI(TAG, "Start to relocate ELF file");

#ifdef CONFIG_ELF_LOADER_MAIN_ARGS
    if (strlen(CONFIG_ELF_LOADER_MAIN_ARGS_STRING) <= 0) {
        ESP_LOGE(TAG, "Failed to check arguments %s",
                 CONFIG_ELF_LOADER_MAIN_ARGS_STRING);
        return -1;
    }

    ret = elf_args_decode(CONFIG_ELF_LOADER_MAIN_ARGS_STRING, &argc, &argv);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to decode arguments %s errno=%d",
                 CONFIG_ELF_LOADER_MAIN_ARGS_STRING, ret);
        return ret;
    }
#else
    argc = 0;
    argv = NULL;
#endif

    ret = esp_elf_init(&elf);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        return ret;
    }

    ret = esp_elf_relocate(&elf, buffer);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Start to run ELF file");

    esp_elf_request(&elf, 0, argc, argv);

    ESP_LOGI(TAG, "Success to exit from ELF file");

    esp_elf_deinit(&elf);

#ifdef CONFIG_ELF_LOADER_MAIN_ARGS
    if (argv) {
        free(argv);
    }
#endif

    return 0;
}
