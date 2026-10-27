/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "esp_elf.h"
#include "unity.h"

extern const uint8_t cpp_lifecycle_start[] asm("_binary_cpp_lifecycle_payload_bin_start");
extern const uint8_t cpp_lifecycle_end[] asm("_binary_cpp_lifecycle_payload_bin_end");

TEST_CASE("C++ ELF constructors run once and destructors run at unload", "[elf_loader][lifecycle]")
{
    esp_elf_t elf;
    volatile int observed_stage = -1;
    char *args[] = { (char *) &observed_stage };

    TEST_ASSERT_EQUAL_INT(0, esp_elf_init(&elf));
    TEST_ASSERT_EQUAL_INT(0, esp_elf_relocate(&elf, cpp_lifecycle_start,
                                              (size_t)(cpp_lifecycle_end - cpp_lifecycle_start)));
    TEST_ASSERT_NOT_EQUAL(0, esp_elf_runtime_base(&elf));

    TEST_ASSERT_EQUAL_INT(0, esp_elf_request(&elf, 0, 1, args));
    TEST_ASSERT_EQUAL_INT(42, observed_stage); /* early ctor precedes global object */

    observed_stage = -1;
    TEST_ASSERT_EQUAL_INT(0, esp_elf_request(&elf, 0, 1, args));
    TEST_ASSERT_EQUAL_INT(42, observed_stage); /* ctor arrays were not rerun */

    observed_stage = -1;
    esp_elf_deinit(&elf);
    TEST_ASSERT_EQUAL_INT(99, observed_stage); /* C++ global dtor executed */
}

void app_main(void)
{
    unity_run_menu();
}
