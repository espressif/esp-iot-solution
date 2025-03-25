/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

    //           _____   _____   __  __ _____ _____
    //     /\   |  __ \ / ____| |  \/  |_   _/ ____|
    //    /  \  | |  | | |      | \  / | | || |
    //   / /\ \ | |  | | |      | |\/| | | || |
    //  / ____ \| |__| | |____  | |  | |_| || |____
    // /_/    \_\_____/ \_____| |_|  |_|_____\_____|
    printf("           _____   _____   __  __ _____ _____\n");
    printf("     /\\   |  __ \\ / ____| |  \\/  |_   _/ ____|\n");
    printf("    /  \\  | |  | | |      | \\  / | | || |\n");
    printf("   / /\\ \\ | |  | | |      | |\\/| | | || |\n");
    printf("  / ____ \\| |__| | |____  | |  | |_| || |____\n");
    printf(" /_/    \\_\\_____/ \\_____| |_|  |_|_____|_____|\n");
    unity_run_menu();
}
