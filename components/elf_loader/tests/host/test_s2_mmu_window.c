/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Run the real ESP32-S2 MMU allocation code against simulated MMU registers.
 * The regression is a search exhaustion bug: if 2+ I-bus pages cannot fit
 * consecutively, the allocator must NOT claim a mapping that never occurred.
 * No physical S2 is required to check this invariant. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR 1
#define CONFIG_ELF_LOADER_SET_MMU 1

volatile uint32_t s2_test_mmu_entries[33];

#include "../../src/soc/esp_elf_esp32s2.c"

static int cache_disable_count;
static int cache_enable_count;

void spi_flash_disable_interrupts_caches_and_other_cpu(void)
{
    assert(cache_disable_count == cache_enable_count);
    ++cache_disable_count;
}

void spi_flash_enable_interrupts_caches_and_other_cpu(void)
{
    assert(cache_disable_count == cache_enable_count + 1);
    ++cache_enable_count;
}

static void init_table(void)
{
    for (size_t i = 0; i < ESP_MMU_IBUS_MAX; ++i) {
        s2_test_mmu_entries[i] = MMU_ACCESS_SPIRAM | 0x777;
    }
    s2_test_mmu_entries[ESP_MMU_IBUS_MAX] = 0xdeadbeef;
    cache_disable_count = 0;
    cache_enable_count = 0;
}

static void assert_cache_restored(int operations)
{
    assert(cache_disable_count == operations);
    assert(cache_enable_count == operations);
    assert(s2_test_mmu_entries[ESP_MMU_IBUS_MAX] == 0xdeadbeef);
}

static void check_fragmented_window_failure(void)
{
    init_table();
    esp_elf_t elf = {0};
    elf.ptext = (void *)0x3f800000;
    elf.text_alloc_size = 2 * MMU_UNIT_SIZE;

    /* Make every other page free, including the final legal page, but
     * leave no two consecutive free pages. */
    for (size_t i = ESP_MMU_IBUS_OFF; i < ESP_MMU_IBUS_MAX; i += 2) {
        s2_test_mmu_entries[i] = MMU_INVALID;
    }
    uint32_t original[ESP_MMU_IBUS_MAX];
    for (size_t i = 0; i < ESP_MMU_IBUS_MAX; ++i) {
        original[i] = s2_test_mmu_entries[i];
    }

    assert(esp_elf_arch_init_mmu(&elf) == -EIO);
    assert(elf.mmu_off == 0 && elf.mmu_num == 0 && elf.text_off == 0);
    for (size_t i = 0; i < ESP_MMU_IBUS_MAX; ++i) {
        assert(s2_test_mmu_entries[i] == original[i]);
    }
    assert_cache_restored(1);
}

static void check_last_legal_window(void)
{
    init_table();
    esp_elf_t elf = {0};
    elf.ptext = (void *)0x3f800000;
    elf.text_alloc_size = 2 * MMU_UNIT_SIZE;

    const uint32_t last = ESP_MMU_IBUS_MAX - 2;
    s2_test_mmu_entries[last] = MMU_INVALID;
    s2_test_mmu_entries[last + 1] = MMU_INVALID;

    assert(esp_elf_arch_init_mmu(&elf) == 0);
    assert(elf.mmu_off == last && elf.mmu_num == 2);
    assert(elf.text_off == ICACHE_ADDR(last) - PSRAM_ALIGN(elf.ptext));
    assert(s2_test_mmu_entries[last] == (MMU_ACCESS_SPIRAM | 0));
    assert(s2_test_mmu_entries[last + 1] == (MMU_ACCESS_SPIRAM | 1));
    assert_cache_restored(1);

    /* Teardown must touch only pages that actually were mapped. */
    esp_elf_arch_deinit_mmu(&elf);
    assert(s2_test_mmu_entries[last] == MMU_INVALID);
    assert(s2_test_mmu_entries[last + 1] == MMU_INVALID);
    assert_cache_restored(2);
}

static void check_unaligned_three_page_allocation(void)
{
    init_table();
    esp_elf_t elf = {0};
    elf.ptext = (void *)0x3f800123;
    elf.text_alloc_size = 2 * MMU_UNIT_SIZE;

    /* 2 complete pages plus an initial 0x123 offset require 3 pages. */
    const uint32_t last = ESP_MMU_IBUS_MAX - 3;
    for (uint32_t i = last; i < ESP_MMU_IBUS_MAX; ++i) {
        s2_test_mmu_entries[i] = MMU_INVALID;
    }
    assert(esp_elf_arch_init_mmu(&elf) == 0);
    assert(elf.mmu_off == last && elf.mmu_num == 3);
    assert(elf.text_off == ICACHE_ADDR(last) - PSRAM_ALIGN(elf.ptext));
    for (uint32_t i = 0; i < 3; ++i) {
        assert(s2_test_mmu_entries[last + i] == (MMU_ACCESS_SPIRAM | i));
    }
    esp_elf_arch_deinit_mmu(&elf);
    for (uint32_t i = 0; i < 3; ++i) {
        assert(s2_test_mmu_entries[last + i] == MMU_INVALID);
    }
    assert_cache_restored(2);
}

static void check_invalid_span_does_not_touch_mmu(void)
{
    init_table();
    esp_elf_t elf = {0};
    elf.ptext = (void *)0x3f800000;
    elf.text_alloc_size = 0;
    assert(esp_elf_arch_init_mmu(&elf) == -EINVAL);
    elf.text_alloc_size = (ESP_MMU_IBUS_MAX - ESP_MMU_IBUS_OFF + 1) *
                          MMU_UNIT_SIZE;
    assert(esp_elf_arch_init_mmu(&elf) == -EINVAL);
    assert(elf.mmu_off == 0 && elf.mmu_num == 0);
    assert_cache_restored(0);
}

int main(void)
{
    check_fragmented_window_failure();
    check_last_legal_window();
    check_unaligned_three_page_allocation();
    check_invalid_span_does_not_touch_mmu();
    return 0;
}
