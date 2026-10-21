/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "private/elf_types.h"
#include "private/elf_validate.h"

#define ELF_PF_EXEC           1
#define ELF_WORD_SIZE         sizeof(uint32_t)

#if CONFIG_IDF_TARGET_ARCH_RISCV
#define ELF_TARGET_MACHINE    243
#elif CONFIG_IDF_TARGET_ARCH_XTENSA
#define ELF_TARGET_MACHINE    94
#else
#error "Unsupported ELF target architecture"
#endif

/* Check ranges using subtraction, not untrusted offset + length. */
static bool elf_file_range(size_t size, size_t offset, size_t length)
{
    return offset <= size && length <= size - offset;
}

static bool elf_file_table(size_t size, size_t offset, size_t count, size_t entry_size)
{
    return offset <= size && count <= (size - offset) / entry_size;
}

/* Every referenced string table has a verified trailing NUL byte. Thus any
 * index within its bounds begins a terminated string without another scan. */
static bool elf_string_in_range(const elf32_shdr_t *str, uint32_t index)
{
    return index < str->size;
}

static elf32_shdr_t elf_section(const uint8_t *pbuf, const elf32_hdr_t *ehdr, size_t index)
{
    elf32_shdr_t shdr;
    memcpy(&shdr, pbuf + ehdr->shoff + index * sizeof(shdr), sizeof(shdr));
    return shdr;
}

static elf32_phdr_t elf_segment(const uint8_t *pbuf, const elf32_hdr_t *ehdr, size_t index)
{
    elf32_phdr_t phdr;
    memcpy(&phdr, pbuf + ehdr->phoff + index * sizeof(phdr), sizeof(phdr));
    return phdr;
}

static bool elf_address_range(uint32_t addr, uint32_t length,
                              uint32_t region, uint32_t region_size)
{
    return addr >= region && (uint64_t)addr + length <= (uint64_t)region + region_size;
}

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
static bool elf_loaded_section(const uint8_t *pbuf, const elf32_shdr_t *shdr,
                               const elf32_shdr_t *shstr)
{
    const char *name = (const char *)pbuf + shstr->offset + shdr->name;

    if (shdr->type == SHT_PROGBITS && (shdr->flags & SHF_ALLOC)) {
        return ((shdr->flags & SHF_EXECINSTR) && strcmp(name, ELF_TEXT) == 0) ||
               ((shdr->flags & SHF_WRITE) && strcmp(name, ELF_DATA) == 0) ||
               strcmp(name, ELF_RODATA) == 0 ||
               strcmp(name, ELF_DATA_REL_RO) == 0;
    }

    return shdr->type == SHT_NOBITS &&
           (shdr->flags & (SHF_ALLOC | SHF_WRITE)) == (SHF_ALLOC | SHF_WRITE) &&
           strcmp(name, ELF_BSS) == 0;
}
#endif

/* Resolve relocation targets only against regions actually loaded by the ELF loader. */
typedef struct {
    uint32_t addr;
    uint32_t size;
    uint32_t flags;
} elf_loaded_range_t;

#if !CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
static int elf_compare_ranges(const void *a, const void *b)
{
    const elf_loaded_range_t *left = (const elf_loaded_range_t *)a;
    const elf_loaded_range_t *right = (const elf_loaded_range_t *)b;

    return (left->addr > right->addr) - (left->addr < right->addr);
}
#endif

#if !CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
static const elf_loaded_range_t *elf_find_loaded_segment(const elf_loaded_range_t *ranges,
                                                         size_t count, uint32_t addr)
{
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (ranges[middle].addr <= addr) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low ? &ranges[low - 1] : NULL;
}
#endif

static bool elf_relocation_target(const elf_loaded_range_t *ranges, size_t count,
                                  uint32_t offset)
{
    if ((offset & (ELF_WORD_SIZE - 1)) != 0) {
        return false;
    }

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    /* Xtensa loads at most ELF_SECS named sections; never rescan the ELF table. */
    for (size_t i = 0; i < count; ++i) {
        if (elf_address_range(offset, ELF_WORD_SIZE, ranges[i].addr, ranges[i].size)) {
            return true;
        }
    }
#else
    /* The RISC-V loader places the lowest virtual PT_LOAD address at the
     * allocation base. A word-aligned virtual r_offset can still become an
     * unaligned host write if that virtual base is not word-aligned. */
    if (!count || offset < ranges[0].addr ||
            ((offset - ranges[0].addr) & (ELF_WORD_SIZE - 1)) != 0) {
        return false;
    }
    /* PT_LOAD ranges are sorted and disjoint: binary search is O(log phnum). */
    const elf_loaded_range_t *segment = elf_find_loaded_segment(ranges, count, offset);
    if (segment) {
        return elf_address_range(offset, ELF_WORD_SIZE, segment->addr, segment->size);
    }
#endif
    return false;
}

/* Exported functions must resolve to loaded executable code, not ELF-controlled
 * addresses outside the loader allocation. */
static bool elf_exported_function(const uint8_t *pbuf, const elf32_hdr_t *ehdr,
                                  const elf32_shdr_t *shstr, const elf32_sym_t *sym,
                                  const elf_loaded_range_t *ranges, size_t range_count)
{
    if (sym->shndx == SHN_UNDEF || sym->shndx >= ehdr->shnum) {
        return false;
    }

    elf32_shdr_t section = elf_section(pbuf, ehdr, sym->shndx);
    if (section.type != SHT_PROGBITS ||
            (section.flags & (SHF_EXECINSTR | SHF_ALLOC)) != (SHF_EXECINSTR | SHF_ALLOC) ||
            !elf_address_range(sym->value, sym->size ? sym->size : 1,
                               section.addr, section.size)) {
        return false;
    }

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    (void)ranges;
    (void)range_count;
    return elf_string_in_range(shstr, section.name) &&
           strcmp((const char *)pbuf + shstr->offset + section.name, ELF_TEXT) == 0;
#else
    (void)shstr;
    const elf_loaded_range_t *segment = elf_find_loaded_segment(ranges, range_count, sym->value);
    return segment && (segment->flags & ELF_PF_EXEC) &&
           elf_address_range(sym->value, sym->size ? sym->size : 1,
                             segment->addr, segment->size);
#endif
}

int esp_elf_validate(const uint8_t *pbuf, size_t size)
{
    elf32_hdr_t ehdr;

    if (!pbuf || !elf_file_range(size, 0, sizeof(ehdr)) ||
            ((uintptr_t)pbuf & (ELF_WORD_SIZE - 1)) != 0) {
        return -EINVAL;
    }

    memcpy(&ehdr, pbuf, sizeof(ehdr));

    if (ehdr.ident[0] != 0x7f || ehdr.ident[1] != 'E' ||
            ehdr.ident[2] != 'L' || ehdr.ident[3] != 'F' ||
            ehdr.ident[4] != 1 || ehdr.ident[5] != 1 || ehdr.ident[6] != 1 ||
            ehdr.version != 1 || ehdr.machine != ELF_TARGET_MACHINE ||
            (ehdr.type != ELF_ET_DYN && ehdr.type != ELF_ET_EXEC) ||
            ehdr.ehsize != sizeof(ehdr) || ehdr.shnum == 0 ||
            ehdr.shentsize != sizeof(elf32_shdr_t) ||
            (ehdr.shoff & (ELF_WORD_SIZE - 1)) ||
            !elf_file_table(size, ehdr.shoff, ehdr.shnum, sizeof(elf32_shdr_t)) ||
            ehdr.shstrndx == 0 || ehdr.shstrndx >= ehdr.shnum ||
            (ehdr.phnum && (ehdr.phentsize != sizeof(elf32_phdr_t) ||
                            (ehdr.phoff & (ELF_WORD_SIZE - 1)) ||
                            !elf_file_table(size, ehdr.phoff, ehdr.phnum, sizeof(elf32_phdr_t))))) {
        return -EINVAL;
    }

    elf32_shdr_t shstr = elf_section(pbuf, &ehdr, ehdr.shstrndx);
    if (shstr.type != SHT_STRTAB || !shstr.size ||
            !elf_file_range(size, shstr.offset, shstr.size) ||
            pbuf[(size_t)shstr.offset + shstr.size - 1] != '\0') {
        return -EINVAL;
    }

    bool executable_entry = false;
    bool has_load_segment = false;
    bool no_entry_shared_object = false;
#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    /* Shared libraries export symbols and do not require an entry point. */
    no_entry_shared_object = ehdr.type == ELF_ET_DYN && ehdr.entry == 0;
#endif
    for (size_t i = 0; i < ehdr.phnum; ++i) {
        elf32_phdr_t phdr = elf_segment(pbuf, &ehdr, i);
        if (phdr.type != PT_LOAD) {
            continue;
        }

        has_load_segment = true;
        if (phdr.filesz > phdr.memsz ||
                !elf_file_range(size, phdr.offset, phdr.filesz) ||
                (uint64_t)phdr.vaddr + phdr.memsz > UINT32_MAX) {
            return -EINVAL;
        }
        /* The entry must contain actual executable file bytes, not BSS. */
        if ((phdr.flags & ELF_PF_EXEC) &&
                elf_address_range(ehdr.entry, 1, phdr.vaddr, phdr.filesz)) {
            executable_entry = true;
        }
    }

#if !CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    if (!has_load_segment || (!executable_entry && !no_entry_shared_object)) {
        return -EINVAL;
    }
#else
    (void)has_load_segment;
    (void)executable_entry;
    executable_entry = false;
#endif

    /*
     * Validate ALL section ranges before following any sh_link references.
     * Linked string tables may appear after the symbol table in the ELF.
     */
    for (size_t i = 0; i < ehdr.shnum; ++i) {
        elf32_shdr_t sec = elf_section(pbuf, &ehdr, i);
        if ((sec.type != SHT_NOBITS && !elf_file_range(size, sec.offset, sec.size)) ||
                ((sec.flags & SHF_ALLOC) && (uint64_t)sec.addr + sec.size > UINT32_MAX)) {
            return -EINVAL;
        }
    }

    /* Section names, symbol references and relocation targets. */
#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    bool seen_dynsym = false;
#endif
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    uint32_t seen_loaded_sections = 0;
    elf_loaded_range_t loaded_ranges[ELF_SECS];
    size_t loaded_count = 0;
    static const char *const loaded_names[ELF_SECS] = {
        ELF_TEXT, ELF_BSS, ELF_DATA, ELF_RODATA, ELF_DATA_REL_RO
    };
#endif
    for (size_t i = 0; i < ehdr.shnum; ++i) {
        elf32_shdr_t sec = elf_section(pbuf, &ehdr, i);
        if (!elf_string_in_range(&shstr, sec.name)) {
            return -EINVAL;
        }

#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
        /* The dynamic loader looks up this section by name: require its type. */
        if (strcmp((const char *)pbuf + shstr.offset + sec.name, ELF_DYNSYM) == 0) {
            if (sec.type != SHT_SYNSYM || seen_dynsym) {
                return -EINVAL;
            }
            seen_dynsym = true;
        }
#endif

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
        if (elf_loaded_section(pbuf, &sec, &shstr)) {
            const char *name = (const char *)pbuf + shstr.offset + sec.name;
            for (size_t j = 0; j < ELF_SECS; ++j) {
                if (strcmp(name, loaded_names[j]) == 0) {
                    uint32_t mask = 1U << j;
                    if (seen_loaded_sections & mask) {
                        return -EINVAL; /* loader maps only the last of duplicate sections */
                    }
                    seen_loaded_sections |= mask;
                    break;
                }
            }
            loaded_ranges[loaded_count].addr = sec.addr;
            loaded_ranges[loaded_count].size = sec.size;
            ++loaded_count;
        }
        if (sec.type == SHT_PROGBITS &&
                (sec.flags & (SHF_ALLOC | SHF_EXECINSTR)) == (SHF_ALLOC | SHF_EXECINSTR) &&
                strcmp((const char *)pbuf + shstr.offset + sec.name, ELF_TEXT) == 0) {
            if (sec.size > UINT32_MAX - 3) {
                return -EINVAL; /* .text allocation rounds up to a 4-byte boundary */
            }
            if (elf_address_range(ehdr.entry, 1, sec.addr, sec.size)) {
                executable_entry = true;
            }
        }
#endif

        if (sec.type == SHT_SYMTAB || sec.type == SHT_SYNSYM) {
            if ((sec.offset & (ELF_WORD_SIZE - 1)) || sec.size % sizeof(elf32_sym_t) ||
                    (sec.type == SHT_SYNSYM && sec.size / sizeof(elf32_sym_t) > UINT16_MAX) ||
                    (sec.entsize && sec.entsize != sizeof(elf32_sym_t)) ||
                    sec.link >= ehdr.shnum) {
                return -EINVAL;
            }

            elf32_shdr_t strtab = elf_section(pbuf, &ehdr, sec.link);
            if (strtab.type != SHT_STRTAB || !strtab.size ||
                    !elf_file_range(size, strtab.offset, strtab.size) ||
                    pbuf[(size_t)strtab.offset + strtab.size - 1] != '\0') {
                return -EINVAL;
            }
            for (size_t j = 0; j < sec.size / sizeof(elf32_sym_t); ++j) {
                elf32_sym_t sym;
                memcpy(&sym, pbuf + sec.offset + j * sizeof(sym), sizeof(sym));
                if (!elf_string_in_range(&strtab, sym.name)) {
                    return -EINVAL;
                }
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
                if (sec.type == SHT_SYNSYM && ELF_ST_BIND(sym.info) == STB_GLOBAL &&
                        ELF_ST_TYPE(sym.info) == STT_FUNC && sym.shndx != SHN_UNDEF &&
                        !elf_exported_function(pbuf, &ehdr, &shstr, &sym, NULL, 0)) {
                    return -EINVAL;
                }
#endif
            }
        }
    }

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    if (!executable_entry && !no_entry_shared_object) {
        return -EINVAL;
    }
#endif

#if !CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    /* Inline storage is enough for normal ELF images; large inputs stay bounded. */
    elf_loaded_range_t inline_ranges[8];
    elf_loaded_range_t *loaded_ranges = inline_ranges;
    size_t loaded_count = 0;
    if (ehdr.phnum > sizeof(inline_ranges) / sizeof(inline_ranges[0])) {
        loaded_ranges = malloc((size_t)ehdr.phnum * sizeof(*loaded_ranges));
        if (!loaded_ranges) {
            return -ENOMEM;
        }
    }

    for (size_t i = 0; i < ehdr.phnum; ++i) {
        elf32_phdr_t phdr = elf_segment(pbuf, &ehdr, i);
        if (phdr.type == PT_LOAD) {
            loaded_ranges[loaded_count].addr = phdr.vaddr;
            loaded_ranges[loaded_count].size = phdr.memsz;
            loaded_ranges[loaded_count].flags = phdr.flags;
            ++loaded_count;
        }
    }
    qsort(loaded_ranges, loaded_count, sizeof(*loaded_ranges), elf_compare_ranges);

    /* The segment loader also rejects overlapping ranges; this check makes
     * the sorted interval index unambiguous. */
    for (size_t i = 1; i < loaded_count; ++i) {
        if ((uint64_t)loaded_ranges[i - 1].addr + loaded_ranges[i - 1].size >
                loaded_ranges[i].addr) {
            if (loaded_ranges != inline_ranges) {
                free(loaded_ranges);
            }
            return -EINVAL;
        }
    }
#endif

    int result = 0;
#if !CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    /* Validate exported functions after indexing PT_LOAD, not once per symbol
     * against the full program-header table. This also bounds adversarial ELF
     * validation time when both tables contain tens of thousands of entries. */
    for (size_t i = 0; i < ehdr.shnum; ++i) {
        elf32_shdr_t sec = elf_section(pbuf, &ehdr, i);
        if (sec.type != SHT_SYNSYM) {
            continue;
        }
        for (size_t j = 0; j < sec.size / sizeof(elf32_sym_t); ++j) {
            elf32_sym_t sym;
            memcpy(&sym, pbuf + sec.offset + j * sizeof(sym), sizeof(sym));
            if (ELF_ST_BIND(sym.info) == STB_GLOBAL &&
                    ELF_ST_TYPE(sym.info) == STT_FUNC && sym.shndx != SHN_UNDEF &&
                    !elf_exported_function(pbuf, &ehdr, &shstr, &sym,
                                           loaded_ranges, loaded_count)) {
                result = -EINVAL;
                goto cleanup_loaded_ranges;
            }
        }
    }
#endif
    for (size_t i = 0; i < ehdr.shnum; ++i) {
        elf32_shdr_t rela_sec = elf_section(pbuf, &ehdr, i);
        if (rela_sec.type != SHT_RELA) {
            continue;
        }

        if ((rela_sec.offset & (ELF_WORD_SIZE - 1)) ||
                rela_sec.size % sizeof(elf32_rela_t) ||
                (rela_sec.entsize && rela_sec.entsize != sizeof(elf32_rela_t)) ||
                rela_sec.link >= ehdr.shnum) {
            result = -EINVAL;
            break;
        }
        elf32_shdr_t symtab = elf_section(pbuf, &ehdr, rela_sec.link);
        if (symtab.type != SHT_SYMTAB && symtab.type != SHT_SYNSYM) {
            result = -EINVAL;
            break;
        }

        size_t sym_count = symtab.size / sizeof(elf32_sym_t);
        for (size_t j = 0; j < rela_sec.size / sizeof(elf32_rela_t); ++j) {
            elf32_rela_t rela;
            memcpy(&rela, pbuf + rela_sec.offset + j * sizeof(rela), sizeof(rela));
            if (ELF_R_SYM(rela.info) >= sym_count ||
                    !elf_relocation_target(loaded_ranges, loaded_count, rela.offset)) {
                result = -EINVAL;
                break;
            }
        }
        if (result) {
            break;
        }
    }

#if !CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
cleanup_loaded_ranges:
    if (loaded_ranges != inline_ranges) {
        free(loaded_ranges);
    }
#endif
    return result;
}
