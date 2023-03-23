/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <esp_log.h>
#include <soc/soc.h>

#ifdef BOOTLOADER_BUILD
#define XZ_BOOT_HEAP_ADDRESS				SOC_DRAM_LOW // (0x3FC7C000 + 0x4000) // for esp32c3 chip, you can use the free space form this address, pay attention your chip for this
static uint8_t* heap_pool = (uint8_t*)XZ_BOOT_HEAP_ADDRESS; // now these memory is not been used, the address have to be 4-byte aligned.
static uint32_t heap_used_offset = 0;

extern uint32_t* _dram_start;

void* malloc(size_t size)
{
	void* p = NULL;

	if (heap_used_offset + size < (uint32_t)&_dram_start) {
		p = &heap_pool[heap_used_offset];
		heap_used_offset += size;
	}
#ifdef	CONFIG_BOOTLOADER_CUSTOM_DEBUG_ON
	ESP_LOGI("c_malloc", "heap_used_offset=%d", heap_used_offset);
#endif
	return p;
}

// If use the xz decompress in bootloader, we don't support free now!
void free(void *ptr)
{

}
#endif