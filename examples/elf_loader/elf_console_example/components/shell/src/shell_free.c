/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_heap_caps.h"
#include "shell_cmd.h"

int free_main(int argc, char **argv)
{
    multi_heap_info_t info;

    heap_caps_get_info(&info, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    printf("%5s %12s %12s %12s\n", "", "total", "used", "free");
    printf("%-5s %12u %12u %12u\n", "dram", info.total_allocated_bytes + info.total_free_bytes,
           info.total_allocated_bytes, info.total_free_bytes);

#ifdef CONFIG_ESP32S3_SPIRAM_SUPPORT
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    printf("%5s %12u %12u %12u\n", "psram", info.total_allocated_bytes + info.total_free_bytes,
           info.total_allocated_bytes, info.total_free_bytes);
#endif

    return 0;
}

void shell_regitser_cmd_free(void)
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "List memory usage information",
        .hint = NULL,
        .func = &free_main,
        .argtable = NULL
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
