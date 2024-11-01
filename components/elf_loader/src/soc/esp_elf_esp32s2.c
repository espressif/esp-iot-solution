/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <sys/errno.h>
#include "esp_idf_version.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp32s2/rom/cache.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp32s2/spiram.h"
#endif
#include "soc/mmu.h"
#include "private/elf_platform.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define PSRAM_VADDR_START   0x3f800000
#else
#define PSRAM_VADDR_START   (DRAM0_CACHE_ADDRESS_HIGH - esp_spiram_get_size())
#endif

#define MMU_INVALID         BIT(14)
#define MMU_UNIT_SIZE       0x10000
#define MMU_REG             ((volatile uint32_t *)DR_REG_MMU_TABLE)

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
#define IRAM0_ADDRESS_LOW   SOC_IRAM0_ADDRESS_LOW
#define IRAM0_ADDRESS_HIGH  SOC_IRAM0_ADDRESS_HIGH
#define MMU_ACCESS_SPIRAM   SOC_MMU_ACCESS_SPIRAM
#endif

#define ESP_MMU_IBUS_BASE   IRAM0_ADDRESS_LOW
#define ESP_MMU_IBUS_MAX    ((IRAM0_ADDRESS_HIGH - IRAM0_ADDRESS_LOW) / \
                             MMU_UNIT_SIZE)
#define ESP_MMU_IBUS_OFF    (8)

#define DADDR_2_SECS(v)     ((v) / MMU_UNIT_SIZE)
#define IADDR_2_SECS(v)     ((v) / MMU_UNIT_SIZE)

#define PSRAM_OFF(v)        ((v) - PSRAM_VADDR_START)
#define PSRAM_SECS(v)       DADDR_2_SECS(PSRAM_OFF((uintptr_t)(v)))
#define PSRAM_ALIGN(v)      ((uintptr_t)(v) & (~(MMU_UNIT_SIZE - 1)))

#define ICACHE_ADDR(s)      (ESP_MMU_IBUS_BASE + (s) * MMU_UNIT_SIZE)

extern void spi_flash_disable_interrupts_caches_and_other_cpu(void);
extern void spi_flash_enable_interrupts_caches_and_other_cpu(void);

/**
 * @brief Initialize MMU hardware remapping function.
 *
 * @param elf - ELF object pointer
 *
 * @return 0 if success or a negative value if failed.
 */
int IRAM_ATTR esp_elf_arch_init_mmu(esp_elf_t *elf)
{
    int ret;
    int off;
    uint32_t ibus_secs;
    volatile uint32_t dbus_secs;
    esp_elf_sec_t *sec = &elf->sec[ELF_SEC_TEXT];
    volatile uint32_t *p = MMU_REG;

    if (sec->size % MMU_UNIT_SIZE) {
        ibus_secs = IADDR_2_SECS(sec->size) + 1;
    } else {
        ibus_secs = IADDR_2_SECS(sec->size);
    }

    dbus_secs = PSRAM_SECS(elf->ptext);

    spi_flash_disable_interrupts_caches_and_other_cpu();

    for (off = ESP_MMU_IBUS_OFF; off < ESP_MMU_IBUS_MAX; off++) {
        if (p[off] == MMU_INVALID) {
            int j;

            for (j = 1; j < ibus_secs; j++) {
                if (p[off + j] != MMU_INVALID) {
                    break;
                }
            }

            if (j >= ibus_secs) {
                for (int k = 0; k < ibus_secs; k++) {
                    p [off + k] = MMU_ACCESS_SPIRAM | (dbus_secs + k);
                }

                break;
            }
        }
    }

    spi_flash_enable_interrupts_caches_and_other_cpu();

    if (off >= ESP_MMU_IBUS_MAX) {
        ret = -EIO;
    } else {
        elf->mmu_off  = off;
        elf->mmu_num  = ibus_secs;
        elf->text_off = ICACHE_ADDR(off) - PSRAM_ALIGN(elf->ptext);

        ret = 0;
    }

    return ret;
}

/**
 * @brief De-initialize MMU hardware remapping function.
 *
 * @param elf - ELF object pointer
 *
 * @return None
 */
void IRAM_ATTR esp_elf_arch_deinit_mmu(esp_elf_t *elf)
{
    volatile int num = elf->mmu_num;
    volatile int off = elf->mmu_off;
    volatile uint32_t *p = MMU_REG;

    spi_flash_disable_interrupts_caches_and_other_cpu();

    for (int i = 0; i < num; i++) {
        p [off + i] = MMU_INVALID;
    }

    spi_flash_enable_interrupts_caches_and_other_cpu();
}
