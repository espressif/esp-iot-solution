/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EI_NIDENT       16              /*!< Magic number and other information length */

/** @brief Type of segment */

#define PT_NULL         0               /*!< Program header table entry unused */
#define PT_LOAD         1               /*!< Loadable program segment */
#define PT_DYNAMIC      2               /*!< Dynamic linking information */
#define PT_INTERP       3               /*!< Program interpreter */
#define PT_NOTE         4               /*!< Auxiliary information */
#define PT_SHLIB        5               /*!< Reserved */
#define PT_PHDR         6               /*!< Entry for header table itself */
#define PT_TLS          7               /*!< Thread-local storage segment */
#define PT_NUM          8               /*!< Number of defined types */
#define PT_LOOS         0x60000000      /*!< Start of OS-specific */
#define PT_GNU_EH_FRAME 0x6474e550      /*!< GCC .eh_frame_hdr segment */
#define PT_GNU_STACK    0x6474e551      /*!< Indicates stack executability */
#define PT_GNU_RELRO    0x6474e552      /*!< Read-only after relocation */
#define PT_LOSUNW       0x6ffffffa
#define PT_SUNWBSS      0x6ffffffa      /*!< Sun Specific segment */
#define PT_SUNWSTACK    0x6ffffffb      /*!< Stack segment */
#define PT_HISUNW       0x6fffffff
#define PT_HIOS         0x6fffffff      /*!< End of OS-specific */
#define PT_LOPROC       0x70000000      /*!< Start of processor-specific */
#define PT_HIPROC       0x7fffffff      /*!< End of processor-specific */

/** @brief Section Type */

#define SHT_NULL        0               /*!< invalid section header */
#define SHT_PROGBITS    1               /*!< some useful section like .text, .data .got, .plt .rodata .interp and so on */
#define SHT_SYMTAB      2               /*!< symbol table */
#define SHT_STRTAB      3               /*!< string table */
#define SHT_RELA        4               /*!< relocation table */
#define SHT_HASH        5               /*!< HASH table */
#define SHT_SYNAMIC     6               /*!< dynamic symbol table */
#define SHT_NOTE        7               /*!< note information */
#define SHT_NOBITS      8               /*!< .bss */
#define SHT_REL         9               /*!< relocation table */
#define SHT_SHKIB       10              /*!< reserved but has unspecified semantics. */
#define SHT_SYNSYM      11              /*!< dynamic symbol */
#define SHT_LOPROC      0x70000000      /*!< reserved for processor-specific semantics */
#define SHT_LOUSER      0x7fffffff      /*!< lower bound of the range of indexes reserved for application programs */
#define SHT_HIUSER      0xffffffff      /*!< upper bound of the range of indexes reserved for application programs. */

/** @brief Section Attribute Flags */

#define SHF_WRITE       1               /*!< writable when task runs */
#define SHF_ALLOC       2               /*!< allocated when task runs */
#define SHF_EXECINSTR   4               /*!< machine code */
#define SHF_MASKPROG    0xf0000000      /*!< reserved for processor-specific semantics */

/** @brief Symbol Types */

#define STT_NOTYPE      0               /*!< symbol type is unspecified */
#define STT_OBJECT      1               /*!< data object */
#define STT_FUNC        2               /*!< code object */
#define STT_SECTION     3               /*!< symbol identifies an ELF section */
#define STT_FILE        4               /*!< symbol's name is file name */
#define STT_COMMON      5               /*!< common data object */
#define STT_TLS         6               /*!< thread-local data object */
#define STT_NUM         7               /*!< defined types in generic range */
#define STT_LOOS        10              /*!< Low OS specific range */
#define STT_HIOS        12              /*!< High OS specific range */
#define STT_LOPROC      13              /*!< processor specific range */
#define STT_HIPROC      15              /*!< processor specific link range */

/** @brief Section names */

#define ELF_BSS         ".bss"          /*!< uninitialized data */
#define ELF_DATA        ".data"         /*!< initialized data */
#define ELF_DEBUG       ".debug"        /*!< debug */
#define ELF_DYNAMIC     ".dynamic"      /*!< dynamic linking information */
#define ELF_DYNSTR      ".dynstr"       /*!< dynamic string table */
#define ELF_DYNSYM      ".dynsym"       /*!< dynamic symbol table */
#define ELF_FINI        ".fini"         /*!< termination code */
#define ELF_GOT         ".got"          /*!< global offset table */
#define ELF_HASH        ".hash"         /*!< symbol hash table */
#define ELF_INIT        ".init"         /*!< initialization code */
#define ELF_REL_DATA    ".rel.data"     /*!< relocation data */
#define ELF_REL_FINI    ".rel.fini"     /*!< relocation termination code */
#define ELF_REL_INIT    ".rel.init"     /*!< relocation initialization code */
#define ELF_REL_DYN     ".rel.dyn"      /*!< relocaltion dynamic link info */
#define ELF_REL_RODATA  ".rel.rodata"   /*!< relocation read-only data */
#define ELF_REL_TEXT    ".rel.text"     /*!< relocation code */
#define ELF_RODATA      ".rodata"       /*!< read-only data */
#define ELF_SHSTRTAB    ".shstrtab"     /*!< section header string table */
#define ELF_STRTAB      ".strtab"       /*!< string table */
#define ELF_SYMTAB      ".symtab"       /*!< symbol table */
#define ELF_TEXT        ".text"         /*!< code */
#define ELF_DATA_REL_RO ".data.rel.ro"  /*!< dynamic read-only data */
#define ELF_PLT         ".plt"          /*!< procedure linkage table. */
#define ELF_GOT_PLT     ".got.plt"      /*!< a table where resolved addresses from external functions are stored */

/** @brief ELF section and symbol operation */

#define ELF_SEC_TEXT            0
#define ELF_SEC_BSS             1
#define ELF_SEC_DATA            2
#define ELF_SEC_RODATA          3
#define ELF_SEC_DRLRO           4
#define ELF_SECS                5

#define ELF_ST_BIND(_i)         ((_i) >> 4)
#define ELF_ST_TYPE(_i)         ((_i) & 0xf)
#define ELF_ST_INFO(_b, _t)     (((_b)<<4) + ((_t) & 0xf))

#define ELF_R_SYM(_i)           ((_i) >> 8)
#define ELF_R_TYPE(_i)          ((unsigned char)(_i))
#define ELF_R_INFO(_s, _t)      (((_s) << 8) + (unsigned char)(_t))

#define ELF_SEC_MAP(_elf, _sec, _addr) \
            ((_elf)->sec[(_sec)].addr - \
             (_elf)->sec[(_sec)].v_addr + \
             (_addr))

typedef unsigned int    Elf32_Addr;
typedef unsigned int    Elf32_Off;
typedef unsigned int    Elf32_Word;
typedef unsigned short  Elf32_Half;
typedef int             Elf32_Sword;

/** @brief ELF Header */

typedef struct elf32_hdr {
    unsigned char   ident[EI_NIDENT];   /*!< ELF Identification */
    Elf32_Half      type;               /*!< object file type */
    Elf32_Half      machine;            /*!< machine */
    Elf32_Word      version;            /*!< object file version */
    Elf32_Addr      entry;              /*!< virtual entry point */
    Elf32_Off       phoff;              /*!< program header table offset */
    Elf32_Off       shoff;              /*!< section header table offset */
    Elf32_Word      flags;              /*!< processor-specific flags */
    Elf32_Half      ehsize;             /*!< ELF header size */
    Elf32_Half      phentsize;          /*!< program header entry size */
    Elf32_Half      phnum;              /*!< number of program header entries */
    Elf32_Half      shentsize;          /*!< section header entry size */
    Elf32_Half      shnum;              /*!< number of section header entries */
    Elf32_Half      shstrndx;           /*!< section header table's "section header string table" entry offset */
} elf32_hdr_t;

/** @brief Program Header */

typedef struct elf32_phdr {
    Elf32_Word type;                         /* segment type */
    Elf32_Off  offset;                       /* segment offset */
    Elf32_Addr vaddr;                        /* virtual address of segment */
    Elf32_Addr paddr;                        /* physical address - ignored? */
    Elf32_Word filesz;                       /* number of bytes in file for seg. */
    Elf32_Word memsz;                        /* number of bytes in mem. for seg. */
    Elf32_Word flags;                        /* flags */
    Elf32_Word align;                        /* memory alignment */
} elf32_phdr_t;

/** @brief Section Header */

typedef struct elf32_shdr {
    Elf32_Word      name;               /*!< section index and it is offset in section .shstrtab */
    Elf32_Word      type;               /*!< type */
    Elf32_Word      flags;              /*!< flags */
    Elf32_Addr      addr;               /*!< start address when map to task space */
    Elf32_Off       offset;             /*!< offset address from file start address */
    Elf32_Word      size;               /*!< size */
    Elf32_Word      link;               /*!< link to another section */
    Elf32_Word      info;               /*!< additional section information */
    Elf32_Word      addralign;          /*!< address align */
    Elf32_Word      entsize;            /*!< index table size */
} elf32_shdr_t;

/** @brief Symbol Table Entry */

typedef struct elf32_sym {
    Elf32_Word      name;               /*!< name - index into string table */
    Elf32_Addr      value;              /*!< symbol value */
    Elf32_Word      size;               /*!< symbol size */
    unsigned char   info;               /*!< type and binding */
    unsigned char   other;              /*!< 0 - no defined meaning */
    Elf32_Half      shndx;              /*!< section header index */
} elf32_sym_t;

/** @brief Relocation entry with implicit addend */

typedef struct elf32_rel {
    Elf32_Addr      offset;             /*!< offset of relocation */
    Elf32_Word      info;               /*!< symbol table index and type */
} elf32_rel_t;

/** @brief Relocation entry with explicit addend */

typedef struct elf32_rela {
    Elf32_Addr      offset;             /*!< offset of relocation */
    Elf32_Word      info;               /*!< symbol table index and type */
    Elf32_Sword     addend;             /*!< Added information */
} elf32_rela_t;

/** @brief ELF section object */

typedef struct esp_elf_sec {
    uintptr_t       v_addr;             /*!< symbol virtual address */
    off_t           offset;             /*!< offset in ELF */

    uintptr_t       addr;               /*!< section physic address in memory */
    size_t          size;               /*!< section size */
} esp_elf_sec_t;

/** @brief ELF object */

typedef struct esp_elf {
    unsigned char   *psegment;          /*!< segment buffer pointer */

    uint32_t         svaddr;            /*!< start virtual address of segment */

    unsigned char   *ptext;             /*!< instruction buffer pointer */

    unsigned char   *pdata;             /*!< data buffer pointer */

    esp_elf_sec_t   sec[ELF_SECS];      /*!< ".bss", "data", "rodata", ".text" */

    int (*entry)(int argc, char *argv[]);               /*!< Entry pointer of ELF */

#ifdef CONFIG_ELF_LOADER_SET_MMU
    uint32_t        text_off;           /* .text symbol offset */

    uint32_t        mmu_off;            /* MMU unit offset */
    uint32_t        mmu_num;            /* MMU unit total number */
#endif
} esp_elf_t;

#ifdef __cplusplus
}
#endif
