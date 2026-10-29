/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Issue #776: execute the exact dlopen -> dlsym -> call -> dlclose path.
 * The original report crashed on ESP32-S3 when the shared object was
 * loaded into PSRAM and dlsym returned its non-executable D-bus pointer.
 */
#include <stdio.h>
#include <stdint.h>

#include "esp_dlfcn.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "esp_memory_utils.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t so_start[] asm("_binary_lib_dlopen_payload_bin_start");
extern const uint8_t so_end[] asm("_binary_lib_dlopen_payload_bin_end");

TEST_CASE("dlopen dlsym executes a loaded shared-library function", "[elf_loader][dlopen]")
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .max_files = 3,
        .format_if_mount_failed = true,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_spiffs_register(&conf));

    /* Copy the original example's lib.so to a real VFS file. Direct
     * esp_elf_relocate does not exercise the buggy dlsym code path. */
    FILE *file = fopen("/storage/lib.so", "wb");
    TEST_ASSERT_NOT_NULL(file);
    size_t so_size = (size_t)(so_end - so_start);
    TEST_ASSERT_GREATER_THAN(0, so_size);
    TEST_ASSERT_EQUAL_UINT32(so_size, fwrite(so_start, 1, so_size, file));
    TEST_ASSERT_EQUAL(0, fclose(file));

#if CONFIG_ELF_LOADER_LOAD_PSRAM
    TEST_ASSERT_GREATER_THAN(0, heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    printf("#776 S3 test: executable .so in PSRAM, available pool=%u bytes\n",
           (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
#else
    printf("#776 S3 test: executable .so in internal IRAM\n");
#endif

    for (int round = 0; round < 3; ++round) {
        void *handle = dlopen("lib.so", RTLD_NOW);
        if (!handle) {
            printf("dlopen failed: %s\n", dlerror());
        }
        TEST_ASSERT_NOT_NULL(handle);

        int (*fibonacci)(int) = (int (*)(int))dlsym(handle, "fibonacci");
        if (!fibonacci) {
            printf("dlsym failed: %s\n", dlerror());
        }
        TEST_ASSERT_NOT_NULL(fibonacci);
        TEST_ASSERT_TRUE(esp_ptr_executable((const void *)fibonacci));

        TEST_ASSERT_NULL(dlsym(handle, "not_exported"));
        TEST_ASSERT_NOT_NULL(dlerror());

        /* S3 PSRAM failed here with InstructionFetchError in Issue #776. */
        TEST_ASSERT_EQUAL_INT(55, fibonacci(10));
        TEST_ASSERT_EQUAL_INT(0, dlclose(handle));
    }

    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_spiffs_unregister("storage"));
}

void app_main(void)
{
#if CONFIG_ELF_LOADER_LOAD_PSRAM
    /* The esp-box-3 CI console provides readable serial output but its USB
     * port does not accept Unity menu input reliably. Run the same Unity
     * case once without requiring a host-initiated serial write. */
    vTaskDelay(pdMS_TO_TICKS(1500));
    UNITY_BEGIN();
    unity_run_all_tests();
    int failures = UNITY_END();

    /* The host may attach after the test completes. Repeat only the result
     * marker, not the SPIFFS setup / module execution. */
    while (true) {
        printf("ELF_DLOPEN_PSRAM=%s\n", failures ? "FAIL" : "PASS");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
#else
    unity_run_menu();
#endif
}
