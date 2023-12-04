/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <esp_log.h>
#include <soc/soc.h>

#ifdef BOOTLOADER_BUILD

#ifdef CONFIG_IDF_TARGET_ESP32
#define XZ_BOOT_HEAP_START_ADDRESS  0x3FFB0000
#define XZ_BOOT_HEAP_END_ADDRESS    0x3FFE0000
#elif (CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6  || CONFIG_IDF_TARGET_ESP32H2)
#define XZ_BOOT_HEAP_START_ADDRESS  SOC_DRAM_LOW
extern uint32_t* _dram_start;
#define XZ_BOOT_HEAP_END_ADDRESS    (uint32_t)&_dram_start
#endif
static uint8_t* heap_pool = (uint8_t*)XZ_BOOT_HEAP_START_ADDRESS; // now these memory is not been used, the address have to be 4-byte aligned.
static uint32_t heap_used_offset = 0;

void* malloc(size_t size)
{
    void* p = NULL;

    if (heap_used_offset + size < XZ_BOOT_HEAP_END_ADDRESS) {
        p = &heap_pool[heap_used_offset];
        heap_used_offset += size;
    }
#ifdef  CONFIG_BOOTLOADER_CUSTOM_DEBUG_ON
    ESP_LOGI("c_malloc", "heap_used_offset=%d", heap_used_offset);
#endif
    return p;
}

// If use the xz decompress in bootloader, we don't support free now!
void free(void *ptr)
{

}
#endif
