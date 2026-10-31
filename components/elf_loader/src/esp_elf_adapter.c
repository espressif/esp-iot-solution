/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/errno.h>
#include "esp_idf_version.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
#include "esp_cache.h"
#endif
#include "soc/soc.h"
#if CONFIG_IDF_TARGET_ESP32S31
#include "esp32s31/rom/cache.h"
#include "soc/cache_reg.h"
#elif CONFIG_IDF_TARGET_ESP32P4
#include "esp32p4/rom/cache.h"
#endif
#include "private/elf_platform.h"

#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define OFFSET_TEXT_VALUE   (SOC_IROM_LOW - SOC_DROM_LOW)
#endif
#endif

/**
 * @brief Allocate block of memory.
 *
 * @param n - Memory size in byte
 * @param exec - True: memory can run executable code; False: memory can R/W data
 *
 * @return Memory pointer if success or NULL if failed.
 */
void *esp_elf_malloc(uint32_t n, bool exec)
{
    uint32_t caps;

#if defined(CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT) && CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT \
    && !defined(CONFIG_ELF_LOADER_LOAD_PSRAM)
    /*
     * With PMP IDRAM split, DRAM is not executable. Without LOAD_PSRAM the
     * loader places .text in ordinary writable SRAM (C5/C6/C61/P4/S31, etc.),
     * so refuse executable allocations early instead of failing at fetch.
     */
    if (exec) {
        static const char *TAG = "elf_adapter";
        ESP_LOGE(TAG, "cannot execute ELF from SRAM when "
                 "CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT=y; disable PMP IDRAM split "
                 "or enable ELF_LOADER_LOAD_PSRAM with SPIRAM");
        return NULL;
    }
#endif

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
    caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#else
#ifdef MALLOC_CAP_EXEC
    caps = exec ? MALLOC_CAP_EXEC : MALLOC_CAP_8BIT;
#else
    caps = MALLOC_CAP_8BIT | MALLOC_CAP_32BIT;
#endif
#endif
#else
#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
    caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#else
    caps = MALLOC_CAP_8BIT;
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    caps |= MALLOC_CAP_CACHE_ALIGNED;
#endif
#endif

    return heap_caps_malloc(n, caps);
}

/**
 * @brief Free block of memory.
 *
 * @param ptr - memory block pointer allocated by "esp_elf_malloc"
 *
 * @return None
 */
void esp_elf_free(void *ptr)
{
    heap_caps_free(ptr);
}

/**
 * @brief Remap symbol from ".data" to ".text" section.
 *
 * @param elf  - ELF object pointer
 * @param sym  - ELF symbol table
 *
 * @return Remapped symbol value
 */
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
uintptr_t elf_remap_text(esp_elf_t *elf, uintptr_t sym)
{
    uintptr_t mapped_sym;
    esp_elf_sec_t *sec = &elf->sec[ELF_SEC_TEXT];

    if ((sym >= sec->addr) &&
            (sym < (sec->addr + sec->size))) {
#ifdef CONFIG_ELF_LOADER_SET_MMU
        mapped_sym = sym + elf->text_off;
#else
        mapped_sym = sym + OFFSET_TEXT_VALUE;
#endif
    } else {
        mapped_sym = sym;
    }

    return mapped_sym;
}
#endif

/**
 * @brief Synchronize ELF text between cache and external RAM.
 *
 * @param elf - ELF object pointer
 *
 * @return 0 if success or a negative value if failed.
 *
 * @note ESP32-S3 on ESP-IDF >= 5.2 writes back the loaded .text range through
 *       D-cache and invalidates the corresponding I-cache range with
 *       esp_cache_msync(). Other supported targets keep the full-cache path.
 */
#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
int IRAM_ATTR esp_elf_arch_flush(esp_elf_t *elf)
{
#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    esp_elf_sec_t *text = &elf->sec[ELF_SEC_TEXT];
    if (text->addr && elf->text_alloc_size) {
        /* .got.loc is part of the executable PC-relative span, even though
         * it is a separate section for symbol lookup. */
        esp_err_t ret = esp_cache_msync((void *)text->addr, elf->text_alloc_size,
                                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        if (ret != ESP_OK) {
            return -EIO;
        }

        const uintptr_t instruction_addr = elf_remap_text(elf, text->addr);
        const uintptr_t line_size = CONFIG_ESP32S3_INSTRUCTION_CACHE_LINE_SIZE;
        const uintptr_t aligned_begin = instruction_addr & ~(line_size - 1);
        const uintptr_t aligned_end = ELF_ALIGN(instruction_addr + elf->text_alloc_size, line_size);
        ret = esp_cache_msync((void *)aligned_begin, aligned_end - aligned_begin,
                              ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_TYPE_INST);
        if (ret != ESP_OK) {
            return -EIO;
        }
    }
#else
    (void)elf;
    extern void spi_flash_disable_interrupts_caches_and_other_cpu(void);
    extern void spi_flash_enable_interrupts_caches_and_other_cpu(void);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

#if CONFIG_IDF_TARGET_ESP32S31
    /* ESP32-S31: Ranged cache APIs (Cache_WriteBack_Addr/Cache_Invalidate_Addr)
     * cause intermittent Instruction access faults (MCAUSE=0x01,
     * MEPC=0x00000000) during long-term ELF execution from PSRAM, likely
     * related to unaligned addr/size (e.g. seg_size=0x136a8, cache_line=64B).
     * Use full D-writeback + I-invalidate like other targets. */
    Cache_WriteBack_All(CACHE_MAP_L1_DCACHE);
    spi_flash_disable_interrupts_caches_and_other_cpu();
    Cache_Invalidate_All(CACHE_MAP_L1_ICACHE_MASK);
    spi_flash_enable_interrupts_caches_and_other_cpu();
    REG_CLR_BIT(CACHE_L1_ICACHE_CTRL_REG, CACHE_L1_ICACHE_SHUT_IBUS1);
#elif CONFIG_IDF_TARGET_ESP32P4
    /* ESP32-P4: the ROM functions take a cache map. The generic declaration
     * below has no argument, which leaves the map undefined, so the code
     * written to PSRAM is not reliably written back from the L1 D-cache and
     * L2 cache and the I-cache is not invalidated (Instruction access fault
     * when the ELF first runs). */
    Cache_WriteBack_All(CACHE_MAP_L1_DCACHE | CACHE_MAP_L2_CACHE);
    spi_flash_disable_interrupts_caches_and_other_cpu();
    Cache_Invalidate_All(CACHE_MAP_L1_ICACHE_MASK);
    spi_flash_enable_interrupts_caches_and_other_cpu();
#else
    extern void Cache_WriteBack_All(void);
    Cache_WriteBack_All();
    spi_flash_disable_interrupts_caches_and_other_cpu();
    spi_flash_enable_interrupts_caches_and_other_cpu();
#endif
#else
    void esp_spiram_writeback_cache(void);

    esp_spiram_writeback_cache();
    spi_flash_disable_interrupts_caches_and_other_cpu();
    spi_flash_enable_interrupts_caches_and_other_cpu();
#endif
#endif

    return 0;
}
#endif
