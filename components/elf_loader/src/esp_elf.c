/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/param.h>

#include "esp_log.h"
#include "soc/soc_caps.h"

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
#include "hal/cache_ll.h"
#endif

#include "private/elf_symbol.h"
#include "private/elf_platform.h"

#define stype(_s, _t)               ((_s)->type == (_t))
#define sflags(_s, _f)              (((_s)->flags & (_f)) == (_f))
#define ADDR_OFFSET                 (0x400)

static const char *TAG = "ELF";

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR

/**
 * @brief Load ELF section.
 *
 * @param elf - ELF object pointer
 * @param pbuf - ELF data buffer
 *
 * @return ESP_OK if success or other if failed.
 */

static int esp_elf_load_section(esp_elf_t *elf, const uint8_t *pbuf)
{
    uint32_t entry;
    uint32_t size;

    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    const elf32_shdr_t *shdr = (const elf32_shdr_t *)(pbuf + ehdr->shoff);
    const char *shstrab = (const char *)pbuf + shdr[ehdr->shstrndx].offset;

    /* Calculate ELF image size */

    for (uint32_t i = 0; i < ehdr->shnum; i++) {
        const char *name = shstrab + shdr[i].name;

        if (stype(&shdr[i], SHT_PROGBITS) && sflags(&shdr[i], SHF_ALLOC)) {
            if (sflags(&shdr[i], SHF_EXECINSTR) && !strcmp(ELF_TEXT, name)) {
                ESP_LOGD(TAG, ".text   sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_TEXT].v_addr  = shdr[i].addr;
                elf->sec[ELF_SEC_TEXT].size    = ELF_ALIGN(shdr[i].size, 4);
                elf->sec[ELF_SEC_TEXT].offset  = shdr[i].offset;

                ESP_LOGD(TAG, ".text   offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_TEXT].offset,
                         elf->sec[ELF_SEC_TEXT].size);
            } else if (sflags(&shdr[i], SHF_WRITE) && !strcmp(ELF_DATA, name)) {
                ESP_LOGD(TAG, ".data   sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_DATA].v_addr  = shdr[i].addr;
                elf->sec[ELF_SEC_DATA].size    = shdr[i].size;
                elf->sec[ELF_SEC_DATA].offset  = shdr[i].offset;

                ESP_LOGD(TAG, ".data   offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_DATA].offset,
                         elf->sec[ELF_SEC_DATA].size);
            } else if (!strcmp(ELF_RODATA, name)) {
                ESP_LOGD(TAG, ".rodata sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_RODATA].v_addr  = shdr[i].addr;
                elf->sec[ELF_SEC_RODATA].size    = shdr[i].size;
                elf->sec[ELF_SEC_RODATA].offset  = shdr[i].offset;

                ESP_LOGD(TAG, ".rodata offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_RODATA].offset,
                         elf->sec[ELF_SEC_RODATA].size);
            } else if (!strcmp(ELF_DATA_REL_RO, name)) {
                ESP_LOGD(TAG, ".data.rel.ro sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_DRLRO].v_addr  = shdr[i].addr;
                elf->sec[ELF_SEC_DRLRO].size    = shdr[i].size;
                elf->sec[ELF_SEC_DRLRO].offset  = shdr[i].offset;

                ESP_LOGD(TAG, ".data.rel.ro offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_DRLRO].offset,
                         elf->sec[ELF_SEC_DRLRO].size);
            }
        } else if (stype(&shdr[i], SHT_NOBITS) &&
                   sflags(&shdr[i], SHF_ALLOC | SHF_WRITE) &&
                   !strcmp(ELF_BSS, name)) {
            ESP_LOGD(TAG, ".bss    sec addr=0x%08x size=0x%08x offset=0x%08x",
                     shdr[i].addr, shdr[i].size, shdr[i].offset);

            elf->sec[ELF_SEC_BSS].v_addr  = shdr[i].addr;
            elf->sec[ELF_SEC_BSS].size    = shdr[i].size;
            elf->sec[ELF_SEC_BSS].offset  = shdr[i].offset;

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

    /* Dump ".text" from ELF to executable space memory */

    elf->sec[ELF_SEC_TEXT].addr = (Elf32_Addr)elf->ptext;
    memcpy(elf->ptext, pbuf + elf->sec[ELF_SEC_TEXT].offset,
           elf->sec[ELF_SEC_TEXT].size);

#ifdef CONFIG_ELF_LOADER_SET_MMU
    if (esp_elf_arch_init_mmu(elf)) {
        esp_elf_free(elf->ptext);
        esp_elf_free(elf->pdata);
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

    entry = ehdr->entry + elf->sec[ELF_SEC_TEXT].addr -
            elf->sec[ELF_SEC_TEXT].v_addr;

#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    elf->entry = (void *)elf_remap_text(elf, (uintptr_t)entry);
#else
    elf->entry = (void *)entry;
#endif

    return 0;
}

#else

/**
 * @brief Load ELF segment.
 *
 * @param elf - ELF object pointer
 * @param pbuf - ELF data buffer
 *
 * @return ESP_OK if success or other if failed.
 */

static int esp_elf_load_segment(esp_elf_t *elf, const uint8_t *pbuf)
{
    uint32_t size;
    bool first_segment = false;
    Elf32_Addr vaddr_s = 0;
    Elf32_Addr vaddr_e = 0;

    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    const elf32_phdr_t *phdr = (const elf32_phdr_t *)(pbuf + ehdr->phoff);

    for (int i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type != PT_LOAD) {
            continue;
        }

        if (phdr[i].memsz < phdr[i].filesz) {
            ESP_LOGE(TAG, "Invalid segment[%d], memsz: %d, filesz: %d",
                     i, phdr[i].memsz, phdr[i].filesz);
            return -EINVAL;
        }

        if (first_segment == true) {
            vaddr_s = phdr[i].vaddr;
            vaddr_e = phdr[i].vaddr + phdr[i].memsz;
            first_segment = true;
            if (vaddr_e < vaddr_s) {
                ESP_LOGE(TAG, "Invalid segment[%d], vaddr: 0x%x, memsz: %d",
                         i, phdr[i].vaddr, phdr[i].memsz);
                return -EINVAL;
            }
        } else {
            if (phdr[i].vaddr < vaddr_e) {
                ESP_LOGE(TAG, "Invalid segment[%d], should not overlap, vaddr: 0x%x, vaddr_e: 0x%x\n",
                         i, phdr[i].vaddr, vaddr_e);
                return -EINVAL;
            }

            if (phdr[i].vaddr > vaddr_e + ADDR_OFFSET) {
                ESP_LOGI(TAG, "Too much padding before segment[%d], padding: %d",
                         i, phdr[i].vaddr - vaddr_e);
            }

            vaddr_e = phdr[i].vaddr + phdr[i].memsz;
            if (vaddr_e < phdr[i].vaddr) {
                ESP_LOGE(TAG, "Invalid segment[%d], address overflow, vaddr: 0x%x, vaddr_e: 0x%x\n",
                         i, phdr[i].vaddr, vaddr_e);
                return -EINVAL;
            }
        }

        ESP_LOGD(TAG, "LOAD segment[%d], vaddr: 0x%x, memsize: 0x%08x",
                 i, phdr[i].vaddr, phdr[i].memsz);
    }

    size = vaddr_e - vaddr_s;
    if (size == 0) {
        return -EINVAL;
    }

    elf->svaddr = vaddr_s;
    elf->psegment = esp_elf_malloc(size, true);
    if (!elf->psegment) {
        return -ENOMEM;
    }

    memset(elf->psegment, 0, size);

    /* Dump "PT_LOAD" from ELF to memory space */

    for (int i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type == PT_LOAD) {
            memcpy(elf->psegment + phdr[i].vaddr - vaddr_s,
                   (uint8_t *)pbuf + phdr[i].offset, phdr[i].filesz);
            ESP_LOGD(TAG, "Copy segment[%d], mem_addr: 0x%x, vaddr: 0x%x, size: 0x%08x",
                     i, (int)((uint8_t *)elf->psegment + phdr[i].vaddr - vaddr_s),
                     phdr[i].vaddr, phdr[i].filesz);
        }
    }

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
    cache_ll_writeback_all(CACHE_LL_LEVEL_INT_MEM, CACHE_TYPE_DATA, CACHE_LL_ID_ALL);
#endif

    elf->entry = (void *)((uint8_t *)elf->psegment + ehdr->entry - vaddr_s);

    return 0;
}
#endif

/**
 * @brief Map symbol's address of ELF to physic space.
 *
 * @param elf - ELF object pointer
 * @param sym - ELF symbol address
 *
 * @return ESP_OK if success or other if failed.
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
 * @return ESP_OK if success or other if failed.
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
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_relocate(esp_elf_t *elf, const uint8_t *pbuf)
{
    int ret;

    const elf32_hdr_t *ehdr;
    const elf32_shdr_t *shdr;
    const char *shstrab;

    if (!elf || !pbuf) {
        return -EINVAL;
    }

    ehdr    = (const elf32_hdr_t *)pbuf;
    shdr    = (const elf32_shdr_t *)(pbuf + ehdr->shoff);
    shstrab = (const char *)pbuf + shdr[ehdr->shstrndx].offset;

    /* Load section or segment to memory space */

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    ret = esp_elf_load_section(elf, pbuf);
#else
    ret = esp_elf_load_segment(elf, pbuf);
#endif

    if (ret) {
        ESP_LOGE(TAG, "Error to load elf file, ret=%d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "elf->entry=%p\n", elf->entry);

    /* Relocation section data */

    for (uint32_t i = 0; i < ehdr->shnum; i++) {
        if (stype(&shdr[i], SHT_RELA)) {
            uint32_t nr_reloc;
            const elf32_rela_t *rela;
            const elf32_sym_t *symtab;
            const char *strtab;

            nr_reloc = shdr[i].size / sizeof(elf32_rela_t);
            rela     = (const elf32_rela_t *)(pbuf + shdr[i].offset);
            symtab   = (const elf32_sym_t *)(pbuf + shdr[shdr[i].link].offset);
            strtab   = (const char *)(pbuf + shdr[shdr[shdr[i].link].link].offset);

            ESP_LOGD(TAG, "Section %s has %d symbol tables", shstrab + shdr[i].name, (int)nr_reloc);

            for (int i = 0; i < nr_reloc; i++) {
                int type;
                uintptr_t addr = 0;
                elf32_rela_t rela_buf;

                memcpy(&rela_buf, &rela[i], sizeof(elf32_rela_t));

                const elf32_sym_t *sym = &symtab[ELF_R_SYM(rela_buf.info)];

                type = ELF_R_TYPE(rela_buf.info);
                if (type == STT_COMMON || type == STT_OBJECT || type == STT_SECTION) {
                    const char *comm_name = strtab + sym->name;

                    if (comm_name[0]) {
                        addr = elf_find_sym(comm_name);

                        if (!addr) {
                            ESP_LOGE(TAG, "Can't find common %s", strtab + sym->name);
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
                            esp_elf_free(elf->pdata);
                            esp_elf_free(elf->ptext);
#else
                            esp_elf_free(elf->psegment);
#endif
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
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
                        esp_elf_free(elf->pdata);
                        esp_elf_free(elf->ptext);
#else
                        esp_elf_free(elf->psegment);
#endif
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
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_request(esp_elf_t *elf, int opt, int argc, char *argv[])
{
    if (!elf || !(elf->entry)) {
        return -EINVAL;
    }

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
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    if (elf->pdata) {
        esp_elf_free(elf->pdata);
        elf->pdata = NULL;
    }

    if (elf->ptext) {
        esp_elf_free(elf->ptext);
        elf->ptext = NULL;
    }
#else
    if (elf->psegment) {
        esp_elf_free(elf->ptext);
        elf->ptext = NULL;
    }
#endif

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
void esp_elf_print_ehdr(const uint8_t *pbuf)
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
 * @brief Print program header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_phdr(const uint8_t *pbuf)
{
    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    const elf32_phdr_t *phdr = (const elf32_phdr_t *)((size_t)pbuf + ehdr->phoff);

    for (int i = 0; i < ehdr->phnum; i++) {
        ESP_LOGI(TAG, "%-40s %d", "type:",                          phdr->type);
        ESP_LOGI(TAG, "%-40s 0x%x", "offset:",                      phdr->offset);
        ESP_LOGI(TAG, "%-40s 0x%x", "vaddr",                        phdr->vaddr);
        ESP_LOGI(TAG, "%-40s 0x%x", "paddr:",                       phdr->paddr);
        ESP_LOGI(TAG, "%-40s %d", "filesz",                         phdr->filesz);
        ESP_LOGI(TAG, "%-40s %d", "memsz",                          phdr->memsz);
        ESP_LOGI(TAG, "%-40s %d", "flags",                          phdr->flags);
        ESP_LOGI(TAG, "%-40s 0x%x", "align",                        phdr->align);

        phdr = (const elf32_phdr_t *)((size_t)phdr + sizeof(elf32_phdr_t));
    }
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
    const elf32_hdr_t *ehdr = (const elf32_hdr_t *)pbuf;
    const elf32_shdr_t *shdr = (const elf32_shdr_t *)((size_t)pbuf + ehdr->shoff);

    for (int i = 0; i < ehdr->shnum; i++) {
        ESP_LOGI(TAG, "%-40s %d", "name:",                          shdr->name);
        ESP_LOGI(TAG, "%-40s %d", "type:",                          shdr->type);
        ESP_LOGI(TAG, "%-40s 0x%x", "flags:",                       shdr->flags);
        ESP_LOGI(TAG, "%-40s %x", "addr",                           shdr->addr);
        ESP_LOGI(TAG, "%-40s %x", "offset:",                        shdr->offset);
        ESP_LOGI(TAG, "%-40s %d", "size",                           shdr->size);
        ESP_LOGI(TAG, "%-40s 0x%x", "link",                         shdr->link);
        ESP_LOGI(TAG, "%-40s %d", "addralign",                      shdr->addralign);
        ESP_LOGI(TAG, "%-40s %d", "entsize",                        shdr->entsize);

        shdr = (const elf32_shdr_t *)((size_t)shdr + sizeof(elf32_shdr_t));
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
