/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <esp_log.h>
#include <soc/soc.h>
#include <soc/soc_caps.h>

#ifdef BOOTLOADER_BUILD

/*
 * XZ allocates the LZMA2 dictionary and I/O buffers from a bump allocator
 * that uses the SRAM below the bootloader image.
 *
 * On targets where IRAM and DRAM share the same address range (ESP32-P4,
 * C5/C6/H2, ...), that region must stop at bootloader_iram_seg_start;
 * growing to _dram_start would overwrite .iram_loader.text (including XZ).
 *
 * On targets with separate IRAM/DRAM (ESP32, C3, S3, ...), _dram_start is
 * the correct DRAM ceiling. The DRAM alias of iram_loader_seg sits just
 * below _dram_start, so a top-down dictionary would overwrite loader code.
 * Those chips keep a simple upward bump.
 *
 * ESP32-P4: the LZMA dictionary is remapped to the non-cacheable alias by
 * the decoder. Small buffers still grow up from the bottom; the dictionary
 * is taken from the top with a gap below loader IRAM so flash I/O into the
 * input buffer cannot sit against dictionary history under L1.
 */
#ifdef CONFIG_IDF_TARGET_ESP32
#define XZ_BOOT_HEAP_START_ADDRESS  0x3FFB0000U
#define XZ_BOOT_HEAP_END_ADDRESS    0x3FFE0000U
#else
#define XZ_BOOT_HEAP_START_ADDRESS  ((uint32_t)SOC_DRAM_LOW)
#if (SOC_DIRAM_IRAM_LOW == SOC_DIRAM_DRAM_LOW)
extern uint32_t bootloader_iram_seg_start;
#define XZ_BOOT_HEAP_END_ADDRESS    ((uint32_t)&bootloader_iram_seg_start)
#else
extern uint32_t _dram_start;
#define XZ_BOOT_HEAP_END_ADDRESS    ((uint32_t)&_dram_start)
#endif
#endif

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define XZ_BOOT_HEAP_ALIGN 64U
#define XZ_BOOT_HEAP_TOP_DOWN 1
#define XZ_BOOT_HEAP_TOP_GAP 4096U
#else
#define XZ_BOOT_HEAP_ALIGN 4U
#define XZ_BOOT_HEAP_TOP_DOWN 0
#endif

static uint8_t *heap_pool = (uint8_t *)XZ_BOOT_HEAP_START_ADDRESS;
static uint32_t heap_low_off;
#if XZ_BOOT_HEAP_TOP_DOWN
static uint32_t heap_high_off;
#endif
static int heap_inited;

static void xz_boot_heap_init(void)
{
    heap_low_off = 0;
#if XZ_BOOT_HEAP_TOP_DOWN
    uint32_t span = XZ_BOOT_HEAP_END_ADDRESS - XZ_BOOT_HEAP_START_ADDRESS;

    heap_high_off = span & ~(XZ_BOOT_HEAP_ALIGN - 1U);
    if (heap_high_off > XZ_BOOT_HEAP_TOP_GAP) {
        heap_high_off -= XZ_BOOT_HEAP_TOP_GAP;
    }
#endif
    heap_inited = 1;
}

static void *xz_boot_heap_alloc(size_t size)
{
    void *p = NULL;
    size_t aligned = (size + (XZ_BOOT_HEAP_ALIGN - 1U)) & ~(size_t)(XZ_BOOT_HEAP_ALIGN - 1U);
    uint32_t off;
#if !XZ_BOOT_HEAP_TOP_DOWN
    uint32_t span = XZ_BOOT_HEAP_END_ADDRESS - XZ_BOOT_HEAP_START_ADDRESS;
#endif

    if (!heap_inited) {
        xz_boot_heap_init();
    }

#if XZ_BOOT_HEAP_TOP_DOWN
    if (aligned >= 32U * 1024U) {
        if (heap_high_off < heap_low_off + (uint32_t)aligned) {
            ESP_LOGE("c_malloc", "xz heap full: large %u low=%u high=%u",
                     (unsigned)aligned, (unsigned)heap_low_off, (unsigned)heap_high_off);
            return NULL;
        }
        heap_high_off -= (uint32_t)aligned;
        return &heap_pool[heap_high_off];
    }

    off = (heap_low_off + (XZ_BOOT_HEAP_ALIGN - 1U)) & ~(XZ_BOOT_HEAP_ALIGN - 1U);
    if (off + (uint32_t)aligned > heap_high_off) {
        ESP_LOGE("c_malloc", "xz heap full: request %u low=%u high=%u",
                 (unsigned)aligned, (unsigned)heap_low_off, (unsigned)heap_high_off);
        return NULL;
    }
#else
    off = (heap_low_off + (XZ_BOOT_HEAP_ALIGN - 1U)) & ~(XZ_BOOT_HEAP_ALIGN - 1U);
    if (off + (uint32_t)aligned > span) {
        ESP_LOGE("c_malloc", "xz heap full: request %u used=%u span=%u",
                 (unsigned)aligned, (unsigned)heap_low_off, (unsigned)span);
        return NULL;
    }
#endif
    p = &heap_pool[off];
    heap_low_off = off + (uint32_t)aligned;
#ifdef CONFIG_BOOTLOADER_CUSTOM_DEBUG_ON
    ESP_LOGI("c_malloc", "size=%u low=%u ptr=%p",
             (unsigned)size, (unsigned)heap_low_off, p);
#endif
    return p;
}

void *malloc(size_t size)
{
    return xz_boot_heap_alloc(size);
}

/* Bump allocator: free is intentionally a no-op in the bootloader. */
void free(void *ptr)
{
    (void)ptr;
}
#endif
