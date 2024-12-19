/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <sys/errno.h>
#include "esp_elf.h"
#include "esp_log.h"
#include "private/elf_platform.h"

/** @brief RISC-V relocations defined by the ABIs */

#define R_RISCV_NONE           0
#define R_RISCV_32             1
#define R_RISCV_64             2
#define R_RISCV_RELATIVE       3
#define R_RISCV_COPY           4
#define R_RISCV_JUMP_SLOT      5
#define R_RISCV_TLS_DTPMOD32   6
#define R_RISCV_TLS_DTPMOD64   7
#define R_RISCV_TLS_DTPREL32   8
#define R_RISCV_TLS_DTPREL64   9
#define R_RISCV_TLS_TPREL32    10
#define R_RISCV_TLS_TPREL64    11
#define R_RISCV_TLS_DESC       12
#define R_RISCV_BRANCH         16
#define R_RISCV_JAL            17
#define R_RISCV_CALL           18
#define R_RISCV_CALL_PLT       19
#define R_RISCV_GOT_HI20       20
#define R_RISCV_TLS_GOT_HI20   21
#define R_RISCV_TLS_GD_HI20    22
#define R_RISCV_PCREL_HI20     23
#define R_RISCV_PCREL_LO12_I   24
#define R_RISCV_PCREL_LO12_S   25
#define R_RISCV_HI20           26
#define R_RISCV_LO12_I         27
#define R_RISCV_LO12_S         28
#define R_RISCV_TPREL_HI20     29
#define R_RISCV_TPREL_LO12_I   30
#define R_RISCV_TPREL_LO12_S   31
#define R_RISCV_TPREL_ADD      32
#define R_RISCV_ADD8           33
#define R_RISCV_ADD16          34
#define R_RISCV_ADD32          35
#define R_RISCV_ADD64          36
#define R_RISCV_SUB8           37
#define R_RISCV_SUB16          38
#define R_RISCV_SUB32          39
#define R_RISCV_SUB64          40
#define R_RISCV_GNU_VTINHERIT  41
#define R_RISCV_GNU_VTENTRY    42
#define R_RISCV_ALIGN          43
#define R_RISCV_RVC_BRANCH     44
#define R_RISCV_RVC_JUMP       45
#define R_RISCV_RVC_LUI        46
#define R_RISCV_RELAX          51
#define R_RISCV_SUB6           52
#define R_RISCV_SET6           53
#define R_RISCV_SET8           54
#define R_RISCV_SET16          55
#define R_RISCV_SET32          56
#define R_RISCV_32_PCREL       57
#define R_RISCV_IRELATIVE      58
#define R_RISCV_PLT32          59

static const char *TAG = "elf_arch";

/**
 * @brief Relocates target architecture symbol of ELF
 *
 * @param elf  - ELF object pointer
 * @param rela - Relocated symbol data
 * @param sym  - ELF symbol table
 * @param addr - Jumping target address
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_arch_relocate(esp_elf_t *elf, const elf32_rela_t *rela,
                          const elf32_sym_t *sym, uint32_t addr)
{
    uint32_t *where;

    assert(elf && rela);

    where = (uint32_t *)((uint8_t *)elf->psegment + rela->offset + elf->svaddr);
    ESP_LOGD(TAG, "type: %d, where=%p addr=0x%x offset=0x%x",
             ELF_R_TYPE(rela->info), where, (int)elf->psegment, (int)rela->offset);

    /* Do relocation based on relocation type */

    switch (ELF_R_TYPE(rela->info)) {
    case R_RISCV_NONE:
        break;
    case R_RISCV_32:
        *where = addr + rela->addend;
        break;
    case R_RISCV_RELATIVE:
        *where = (Elf32_Addr)((uint8_t *)elf->psegment - elf->svaddr + rela->addend);
        break;
    case R_RISCV_JUMP_SLOT:
        *where = addr;
        break;
    default:
        ESP_LOGE(TAG, "info=%d is not supported\n", ELF_R_TYPE(rela->info));
        return -EINVAL;
    }

    return 0;
}
