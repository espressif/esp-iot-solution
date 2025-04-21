/* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

    /* _____ _____ _   _ _____  _   _  ______ _   _ _____ _____ _____ _   _
     *|_   _|  _  | | | /  __ \| | | | | ___ \ | | |_   _|_   _|  _  | \ | |
     *  | | | | | | | | | /  \/| |_| | | |_/ / | | | | |   | | | | | |  \| |
     *  | | | | | | | | | |    |  _  | | ___ \ | | | | |   | | | | | | . ` |
     *  | | \ \_/ / |_| | \__/\| | | | | |_/ / |_| | | |   | | \ \_/ / |\  |
     *  \_/  \___/ \___/ \____/\_| |_/ \____/ \___/  \_/   \_/  \___/\_| \_/
     */
    printf(" _____ _____ _   _ _____  _   _  ______ _   _ _____ _____ _____ _   _ \n");
    printf("|_   _|  _  | | | /  __ \\| | | | | ___ \\ | | |_   _|_   _|  _  | \\ | |\n");
    printf("  | | | | | | | | | /  \\/| |_| | | |_/ / | | | | |   | | | | | |  \\| |\n");
    printf("  | | | | | | | | | |    |  _  | | ___ \\ | | | | |   | | | | | | . ` |\n");
    printf("  | | \\ \\_/ / |_| | \\__/\\| | | | | |_/ / |_| | | |   | | \\ \\_/ / |\\  |\n");
    printf("  \\_/  \\___/ \\___/ \\____/\\_| |_/ \\____/ \\___/  \\_/   \\_/  \\___/\\_| \\_/\n");
    unity_run_menu();
}
