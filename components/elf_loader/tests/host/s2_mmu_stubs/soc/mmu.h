/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>

/* The fake MMU register array includes an extra sentinel after its 32
 * legal I-bus entries to detect writes beyond the mapped window. */
extern volatile uint32_t s2_test_mmu_entries[33];
#define DR_REG_MMU_TABLE ((uintptr_t)s2_test_mmu_entries)
#define BIT(n) (1u << (n))
#define SOC_IRAM0_ADDRESS_LOW  0x40080000u
#define SOC_IRAM0_ADDRESS_HIGH 0x40280000u
#define SOC_MMU_ACCESS_SPIRAM  0x100u
