/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

#define LEAKS (400)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(LEAKS);
}

void app_main(void)
{
    /*
    * ____          _    _                  _______          _
    *|  _ \        | |  | |                |__   __|        | |
    *| |_) | _   _ | |_ | |_  ___   _ __      | |  ___  ___ | |_
    *|  _ < | | | || __|| __|/ _ \ | '_ \     | | / _ \/ __|| __|
    *| |_) || |_| || |_ | |_| (_) || | | |    | ||  __/\__ \| |_
    *|____/  \__,_| \__| \__|\___/ |_| |_|    |_| \___||___/ \__|
    */
    printf("  ____          _    _                  _______          _   \n");
    printf(" |  _ \\        | |  | |                |__   __|        | |  \n");
    printf(" | |_) | _   _ | |_ | |_  ___   _ __      | |  ___  ___ | |_ \n");
    printf(" |  _ < | | | || __|| __|/ _ \\ | '_ \\     | | / _ \\/ __|| __|\n");
    printf(" | |_) || |_| || |_ | |_| (_) || | | |    | ||  __/\\__ \\| |_ \n");
    printf(" |____/  \\__,_| \\__| \\__|\\___/ |_| |_|    |_| \\___||___/ \\__|\n");
    unity_run_menu();
}
