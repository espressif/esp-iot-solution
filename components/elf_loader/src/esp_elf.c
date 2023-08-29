/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/param.h>

#include "esp_log.h"

#include "private/elf_symbol.h"
#include "private/elf_platform.h"

#define stype(_s, _t)               ((_s)->type == (_t))
#define sflags(_s, _f)              (((_s)->flags & (_f)) == (_f))

static const char *TAG = "ELF";

/**
 * @brief Map symbol's address of ELF to physic space.
 *
 * @param elf - ELF object pointer
 * @param sym - ELF symbol address
 * 
 * @return ESP_OK if sucess or other if failed.
 */
uintptr_t esp_elf_map_sym(esp_elf_t *elf, uintptr_t sym)
{
    for (int i = 0; i < ELF_SECS; i++) {
        if ((sym >= elf->sec[i].v_addr) &&
                (sym < (elf->sec[i].v_addr + elf->sec[i].size))) {
            return sym - elf->sec[i].v_addr + elf->sec[i].addr;
        }
    }

    return 0;
}

/**
 * @brief Initialize ELF object.
 *
 * @param elf - ELF object pointer
 * 
 * @return ESP_OK if sucess or other if failed.
 */
int esp_elf_init(esp_elf_t *elf)
{
    ESP_LOGI(TAG, "ELF loader version: %d.%d.%d", ELF_LOADER_VER_MAJOR, ELF_LOADER_VER_MINOR, ELF_LOADER_VER_PATCH);

    if (!elf) {
        return -EINVAL;
    }

    memset(elf, 0, sizeof(esp_elf_t));

    return 0;
}

/**
 * @brief Decode and relocate ELF data.
 *
 * @param elf - ELF object pointer
 * @param pbuf - ELF data buffer
 * 
 * @return ESP_OK if sucess or other if failed.
 */
int esp_elf_relocate(esp_elf_t *elf, const uint8_t *pbuf)
{
    uint32_t entry;
    uint32_t size;

    const elf32_hdr_t *phdr;
    const elf32_shdr_t *pshdr;
    const char *shstrab;

    if (!elf || !pbuf) {
        return -EINVAL;
    }

    phdr    = (const elf32_hdr_t *)pbuf;
    pshdr   = (const elf32_shdr_t *)(pbuf + phdr->shoff);
    shstrab = (const char *)pbuf + pshdr[phdr->shstrndx].offset;

    /* Calculate ELF image size */

    for (uint32_t i = 0; i < phdr->shnum; i++) {
        const char *name = shstrab + pshdr[i].name;

        if (stype(&pshdr[i], SHT_PROGBITS) && sflags(&pshdr[i], SHF_ALLOC)) {
            if (sflags(&pshdr[i], SHF_EXECINSTR) && !strcmp(ELF_TEXT, name)) {
                ESP_LOGD(TAG, ".text   sec addr=0x%08x size=0x%08x offset=0x%08x",
                         pshdr[i].addr, pshdr[i].size, pshdr[i].offset);

                elf->sec[ELF_SEC_TEXT].v_addr  = pshdr[i].addr;
                elf->sec[ELF_SEC_TEXT].size    = ELF_ALIGN(pshdr[i].size);
                elf->sec[ELF_SEC_TEXT].offset  = pshdr[i].offset;

                ESP_LOGD(TAG, ".text   offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_TEXT].offset,
                         elf->sec[ELF_SEC_TEXT].size);
            } else if (sflags(&pshdr[i], SHF_WRITE) && !strcmp(ELF_DATA, name)) {
                ESP_LOGD(TAG, ".data   sec addr=0x%08x size=0x%08x offset=0x%08x",
                         pshdr[i].addr, pshdr[i].size, pshdr[i].offset);

                elf->sec[ELF_SEC_DATA].v_addr  = pshdr[i].addr;
                elf->sec[ELF_SEC_DATA].size    = pshdr[i].size;
                elf->sec[ELF_SEC_DATA].offset  = pshdr[i].offset;

                ESP_LOGD(TAG, ".data   offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_DATA].offset,
                         elf->sec[ELF_SEC_DATA].size);
            } else if (!strcmp(ELF_RODATA, name)) {
                ESP_LOGD(TAG, ".rodata sec addr=0x%08x size=0x%08x offset=0x%08x",
                         pshdr[i].addr, pshdr[i].size, pshdr[i].offset);

                elf->sec[ELF_SEC_RODATA].v_addr  = pshdr[i].addr;
                elf->sec[ELF_SEC_RODATA].size    = pshdr[i].size;
                elf->sec[ELF_SEC_RODATA].offset  = pshdr[i].offset;

                ESP_LOGD(TAG, ".rodata offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_RODATA].offset,
                         elf->sec[ELF_SEC_RODATA].size);
            } else if (!strcmp(ELF_DATA_REL_RO, name)) {
                ESP_LOGD(TAG, ".data.rel.ro sec addr=0x%08x size=0x%08x offset=0x%08x",
                         pshdr[i].addr, pshdr[i].size, pshdr[i].offset);

                elf->sec[ELF_SEC_DRLRO].v_addr  = pshdr[i].addr;
                elf->sec[ELF_SEC_DRLRO].size    = pshdr[i].size;
                elf->sec[ELF_SEC_DRLRO].offset  = pshdr[i].offset;

                ESP_LOGD(TAG, ".data.rel.ro offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_DRLRO].offset,
                         elf->sec[ELF_SEC_DRLRO].size);
            }
        } else if (stype(&pshdr[i], SHT_NOBITS) &&
                   sflags(&pshdr[i], SHF_ALLOC | SHF_WRITE) &&
                   !strcmp(ELF_BSS, name)) {
            ESP_LOGD(TAG, ".bss    sec addr=0x%08x size=0x%08x offset=0x%08x",
                     pshdr[i].addr, pshdr[i].size, pshdr[i].offset);

            elf->sec[ELF_SEC_BSS].v_addr  = pshdr[i].addr;
            elf->sec[ELF_SEC_BSS].size    = pshdr[i].size;
            elf->sec[ELF_SEC_BSS].offset  = pshdr[i].offset;

            ESP_LOGD(TAG, ".bss    offset is 0x%lx size is 0x%x",
                     elf->sec[ELF_SEC_BSS].offset,
                     elf->sec[ELF_SEC_BSS].size);
        }
    }

    /* No .text on image */

    if (!elf->sec[ELF_SEC_TEXT].size) {
        return -EINVAL;
    }

    elf->ptext = esp_elf_malloc(elf->sec[ELF_SEC_TEXT].size, true);
    if (!elf->ptext) {
        return -ENOMEM;
    }

    size = elf->sec[ELF_SEC_DATA].size +
           elf->sec[ELF_SEC_RODATA].size +
           elf->sec[ELF_SEC_BSS].size +
           elf->sec[ELF_SEC_DRLRO].size;
    if (size) {
        elf->pdata = esp_elf_malloc(size, false);
        if (!elf->pdata) {
            esp_elf_free(elf->ptext);
            return -ENOMEM;
        }
    }

    /* Dump ".text" from ELF to excutable space memory */

    elf->sec[ELF_SEC_TEXT].addr = (Elf32_Addr)elf->ptext;
    memcpy(elf->ptext, pbuf + elf->sec[ELF_SEC_TEXT].offset,
           elf->sec[ELF_SEC_TEXT].size);

#ifdef CONFIG_ELF_LOADER_SET_MMU
    if (esp_elf_arch_init_mmu(elf)) {
        esp_elf_free(elf->ptext);
        return -EIO;
    }
#endif

    /**
     * Dump ".data", ".rodata" and ".bss" from ELF to R/W space memory.
     *
     * Todo: Dump ".rodata" to rodata section by MMU/MPU.
     */

    if (size) {
        uint8_t *pdata = elf->pdata;

        if (elf->sec[ELF_SEC_DATA].size) {
            elf->sec[ELF_SEC_DATA].addr = (uint32_t)pdata;

            memcpy(pdata, pbuf + elf->sec[ELF_SEC_DATA].offset,
                   elf->sec[ELF_SEC_DATA].size);

            pdata += elf->sec[ELF_SEC_DATA].size;
        }

        if (elf->sec[ELF_SEC_RODATA].size) {
            elf->sec[ELF_SEC_RODATA].addr = (uint32_t)pdata;

            memcpy(pdata, pbuf + elf->sec[ELF_SEC_RODATA].offset,
                   elf->sec[ELF_SEC_RODATA].size);

            pdata += elf->sec[ELF_SEC_RODATA].size;
        }

        if (elf->sec[ELF_SEC_DRLRO].size) {
            elf->sec[ELF_SEC_DRLRO].addr = (uint32_t)pdata;

            memcpy(pdata, pbuf + elf->sec[ELF_SEC_DRLRO].offset,
                   elf->sec[ELF_SEC_DRLRO].size);

            pdata += elf->sec[ELF_SEC_DRLRO].size;
        }

        if (elf->sec[ELF_SEC_BSS].size) {
            elf->sec[ELF_SEC_BSS].addr = (uint32_t)pdata;
            memset(pdata, 0, elf->sec[ELF_SEC_BSS].size);
        }
    }

    /* Set ELF entry */

    entry = phdr->entry + elf->sec[ELF_SEC_TEXT].addr -
                          elf->sec[ELF_SEC_TEXT].v_addr;

#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    elf->entry = (void *)elf_remap_text(elf, (uintptr_t)entry);
#else
    elf->entry = (void *)entry;
#endif

    /* Relocation section data */

    for (uint32_t i = 0; i < phdr->shnum; i++) {
        if (stype(&pshdr[i], SHT_RELA)) {
            uint32_t nr_reloc;
            const elf32_rela_t *rela;
            const elf32_sym_t *symtab;
            const char *strtab;

            nr_reloc = pshdr[i].size / sizeof(elf32_rela_t);
            rela     = (const elf32_rela_t *)(pbuf + pshdr[i].offset);
            symtab   = (const elf32_sym_t *)(pbuf + pshdr[pshdr[i].link].offset);
            strtab   = (const char *)(pbuf + pshdr[pshdr[pshdr[i].link].link].offset);

            ESP_LOGD(TAG, "Section %s has %d symbol tables", shstrab + pshdr[i].name, (int)nr_reloc);

            for (int i = 0; i < nr_reloc; i++) {
                int type;
                uintptr_t addr = 0;
                elf32_rela_t rela_buf;

                memcpy(&rela_buf, &rela[i], sizeof(elf32_rela_t));

                const elf32_sym_t *sym = &symtab[ELF_R_SYM(rela_buf.info)];

                type = ELF_R_TYPE(rela->info);
                if (type == STT_COMMON) {
                    const char *comm_name = strtab + sym->name;

                    if (comm_name[0]) {
                        addr = elf_find_sym(comm_name);

                        if (!addr) {
                            ESP_LOGE(TAG, "Can't find common %s", strtab + sym->name);
                            esp_elf_free(elf->pdata);
                            esp_elf_free(elf->ptext);
                            return -ENOSYS;
                        }

                        ESP_LOGD(TAG, "Find common %s addr=%x", comm_name, addr);
                    }
                } else if (type == STT_FILE) {
                    const char *func_name = strtab + sym->name;

                    if (sym->value) {
                        addr = esp_elf_map_sym(elf, sym->value);
                    } else {
                        addr = elf_find_sym(func_name);
                    }

                    if (!addr) {
                        ESP_LOGE(TAG, "Can't find symbol %s", func_name);
                        esp_elf_free(elf->pdata);
                        esp_elf_free(elf->ptext);
                        return -ENOSYS;
                    }

                    ESP_LOGD(TAG, "Find function %s addr=%x", func_name, addr);
                }

                esp_elf_arch_relocate(elf, &rela_buf, sym, addr);
            }
        }
    }

#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
    esp_elf_arch_flush();
#endif

    return 0;
}

/**
 * @brief Request running relocated ELF function.
 *
 * @param elf  - ELF object pointer
 * @param opt  - Request options
 * @param argc - Arguments number
 * @param argv - Arguments value array
 * 
 * @return ESP_OK if sucess or other if failed.
 */
int esp_elf_request(esp_elf_t *elf, int opt, int argc, char *argv[])
{
    elf->entry(argc, argv);

    return 0;
}

/**
 * @brief Deinitialize ELF object.
 *
 * @param elf - ELF object pointer
 * 
 * @return None
 */
void esp_elf_deinit(esp_elf_t *elf)
{
    if (elf->pdata) {
        esp_elf_free(elf->pdata);
        elf->pdata = NULL;
    }

    if (elf->ptext) {
        esp_elf_free(elf->ptext);
        elf->ptext = NULL;
    }

#ifdef CONFIG_ELF_LOADER_SET_MMU
    esp_elf_arch_deinit_mmu(elf);
#endif
}

/**
 * @brief Print header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 * 
 * @return None
 */
void esp_elf_print_hdr(const uint8_t *pbuf)
{
    const char *s_bits, *s_endian;
    const elf32_hdr_t *hdr = (const elf32_hdr_t *)pbuf;

    switch (hdr->ident[4]) {
    case 1:
        s_bits = "32-bit";
        break;
    case 2:
        s_bits = "64-bit";
        break;
    default:
        s_bits = "invalid bits";
        break;
    }

    switch (hdr->ident[5]) {
    case 1:
        s_endian = "little-endian";
        break;
    case 2:
        s_endian = "big-endian";
        break;
    default:
        s_endian = "invalid endian";
        break;
    }

    if (hdr->ident[0] == 0x7f) {
        ESP_LOGI(TAG, "%-40s %c%c%c", "Class:",                     hdr->ident[1], hdr->ident[2], hdr->ident[3]);
    }

    ESP_LOGI(TAG, "%-40s %s, %s", "Format:",                        s_bits, s_endian);
    ESP_LOGI(TAG, "%-40s %x", "Version(current):",                  hdr->ident[6]);

    ESP_LOGI(TAG, "%-40s %d", "Type:",                              hdr->type);
    ESP_LOGI(TAG, "%-40s %d", "Machine:",                           hdr->machine);
    ESP_LOGI(TAG, "%-40s %x", "Version:",                           hdr->version);
    ESP_LOGI(TAG, "%-40s %x", "Entry point address:",               hdr->entry);
    ESP_LOGI(TAG, "%-40s %x", "Start of program headers:",          hdr->phoff);
    ESP_LOGI(TAG, "%-40s %d", "Start of section headers:",          hdr->shoff);
    ESP_LOGI(TAG, "%-40s 0x%x", "Flags:",                           hdr->flags);
    ESP_LOGI(TAG, "%-40s %d", "Size of this header(bytes):",        hdr->ehsize);
    ESP_LOGI(TAG, "%-40s %d", "Size of program headers(bytes):",    hdr->phentsize);
    ESP_LOGI(TAG, "%-40s %d", "Number of program headers:",         hdr->phnum);
    ESP_LOGI(TAG, "%-40s %d", "Size of section headers(bytes):",    hdr->shentsize);
    ESP_LOGI(TAG, "%-40s %d", "Number of section headers:",         hdr->shnum);
    ESP_LOGI(TAG, "%-40s %d", "Section header string table i:",     hdr->shstrndx);
}

/**
 * @brief Print section header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 * 
 * @return None
 */
void esp_elf_print_shdr(const uint8_t *pbuf)
{
    const elf32_hdr_t *phdr = (const elf32_hdr_t *)pbuf;
    const elf32_shdr_t *pshdr = (const elf32_shdr_t *)((size_t)pbuf + phdr->shoff);

    for (int i = 0; i < phdr->shnum; i++) {
        ESP_LOGI(TAG, "%-40s %d", "name:",                          pshdr->name);
        ESP_LOGI(TAG, "%-40s %d", "type:",                          pshdr->type);
        ESP_LOGI(TAG, "%-40s 0x%x", "flags:",                       pshdr->flags);
        ESP_LOGI(TAG, "%-40s %x", "addr",                           pshdr->addr);
        ESP_LOGI(TAG, "%-40s %x", "offset:",                        pshdr->offset);
        ESP_LOGI(TAG, "%-40s %d", "size",                           pshdr->size);
        ESP_LOGI(TAG, "%-40s 0x%x", "link",                         pshdr->link);
        ESP_LOGI(TAG, "%-40s %d", "addralign",                      pshdr->addralign);
        ESP_LOGI(TAG, "%-40s %d", "entsize",                        pshdr->entsize);

        pshdr = (const elf32_shdr_t *)((size_t)pshdr + sizeof(elf32_shdr_t));
    }
}

/**
 * @brief Print section information of ELF.
 *
 * @param pbuf - ELF data buffer
 * 
 * @return None
 */
void esp_elf_print_sec(esp_elf_t *elf)
{
    const char *sec_names[ELF_SECS] = {
        "text", "bss", "data", "rodata"
    };

    for (int i = 0; i < ELF_SECS; i++) {
        ESP_LOGI(TAG, "%s:   0x%08x size 0x%08x", sec_names[i], elf->sec[i].addr, elf->sec[i].size);
    }

    ESP_LOGI(TAG, "entry:  %p", elf->entry);
}
