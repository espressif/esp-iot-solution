/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <inttypes.h>

#include <fcntl.h>

#include "esp_log.h"
#include "esp_elf.h"
#include "soc/soc_caps.h"

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
#include "hal/cache_ll.h"
#endif

#include "private/elf_platform.h"
#include "private/elf_lifecycle.h"
#include "private/elf_validate.h"

#define stype(_s, _t)               ((_s)->type == (_t))
#define sflags(_s, _f)              (((_s)->flags & (_f)) == (_f))
#define ADDR_OFFSET                 (0x400)

#ifdef CONFIG_ELF_LOADER_NUMBER_SYMBOLS
#define SYMBOL_TABLES_NO            CONFIG_ELF_LOADER_NUMBER_SYMBOLS
#else
#define SYMBOL_TABLES_NO            (32)
#endif

#ifdef CONFIG_ELF_FILE_SYSTEM_BASE_PATH
#define FS_PATH                     CONFIG_ELF_FILE_SYSTEM_BASE_PATH
#else
#define FS_PATH                     "/storage"
#endif

static const char *TAG = "ELF";
static esp_elf_symbol_table_t *g_symbol_tables[SYMBOL_TABLES_NO];
static _Atomic(symbol_resolver) current_resolver = elf_find_sym_default;

/* The allocation API uses uint32_t and read() uses a signed byte count. */
static bool esp_elf_file_size_valid(off_t size)
{
    return size > 0 && (uint64_t)size <= INT32_MAX;
}

/* Only an entry-less ET_DYN is a shared library without an entry function. */
static bool esp_elf_is_entryless_shared_object(const elf32_hdr_t *ehdr)
{
#ifdef CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    return ehdr->type == ELF_ET_DYN && ehdr->entry == 0;
#else
    (void)ehdr;
    return false;
#endif
}

/**
 * @brief Open and load an ELF file into memory.
 *
 * @param file - Pointer to elf_file_t structure to store loaded file content
 * @param name - Filename (without path) of the ELF file to open
 *
 * @return 0 on success, -1 on failure with errno set. Error cases include:
 *         - Invalid parameters
 *         - Path generation failure
 *         - File open/read errors
 *         - Memory allocation failures
 *
 * @note The actual file path will be constructed as "FS_PATH/name"
 * @note Allocates memory for file content using esp_elf_malloc()
 */
int esp_elf_open(elf_file_t *file, const char *name)
{
    ssize_t ret;
    int fd;
    char *file_path;
    off_t size;
    uint8_t *pbuf;

    if (!file || !name) {
        errno = EINVAL;
        return -1;
    }

    ret = asprintf(&file_path, FS_PATH"/%s", name);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to generate path errno=%d", errno);
        return -1;
    }

    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file %s errno=%d", file_path, errno);
        goto errout_open_file;
    }

    size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        ESP_LOGE(TAG, "Failed to seek file %s errno=%d", file_path, errno);
        goto errout_lseek_end;
    }
    /* Ensure the file length fits both the allocator and the signed read length. */
    if (!esp_elf_file_size_valid(size)) {
        ESP_LOGE(TAG, "Invalid ELF file size: %" PRId64, (int64_t)size);
        errno = EFBIG;
        goto errout_lseek_end;
    }

    ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) {
        ESP_LOGE(TAG, "Failed to seek file %s errno=%d", file_path, errno);
        goto errout_lseek_end;
    }

    pbuf = esp_elf_malloc(size, false);
    if (!pbuf) {
        ESP_LOGE(TAG, "Failed to malloc %" PRId64 " bytes", (int64_t)size);
        goto errout_lseek_end;
    }

    ret = read(fd, pbuf, size);
    if (ret != (ssize_t)size) {
        ESP_LOGE(TAG, "Failed to read ret=%zd", ret);
        goto errout_read_fs;
    }

    free(file_path);
    close(fd);
    file->payload = pbuf;
    file->size = size;

    return 0;

errout_read_fs:
    esp_elf_free(pbuf);
errout_lseek_end:
    close(fd);
errout_open_file:
    free(file_path);
    return -1;
}

/**
 * @brief Close ELF file and release associated resources.
 *
 * @param file - Pointer to opened elf_file_t structure
 *
 * @note Releases memory allocated by esp_elf_open() for payload data
 * @note Should be called paired with esp_elf_open() to prevent memory leaks
 * @note If file is NULL, this function does nothing (null-safe)
 */
void esp_elf_close(elf_file_t *file)
{
    if (!file) {
        return;
    }

    esp_elf_free(file->payload);
    file->payload = NULL;
    file->size = 0;
}

/**
 * @brief Find symbol address by name.
 *
 * @param sym_name - Symbol name
 *
 * @return Symbol address if success or 0 if failed.
 */
uintptr_t elf_find_sym(const char *sym_name)
{
    if (!sym_name) {
        ESP_LOGE(TAG, "Invalid parameter: sym_name is NULL");
        return 0;
    }

    symbol_resolver resolver = atomic_load(&current_resolver);
    return resolver(sym_name);
}

/* Size of the byte-addressable staging buffer used to fill executable
 * memory from an elf_source_t. Must be 4-byte aligned; bounded so a text
 * load can never fragment-fail on a small heap. */
#define ELF_SOURCE_STAGE_CHUNK     (512)

static int elf_source_read(const elf_source_t *src, size_t offset, void *buf, size_t len)
{
    if (!src || !src->read) {
        return -EINVAL;
    }

    if (!len) {
        return 0;
    }

    return src->read(src->ctx, offset, buf, len);
}

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR

/* MALLOC_CAP_EXEC can return instruction RAM which only accepts 32-bit
 * loads/stores on Xtensa (e.g. ESP32-S3 internal IRAM). libc memcpy() may
 * emit byte stores and panic with LoadStoreError even for aligned pointers.
 * The allocation and literal-pool offsets are word-aligned; store complete
 * words and zero-pad the final partial word without reading past the ELF. */
#ifndef CONFIG_ELF_LOADER_LOAD_PSRAM
static void esp_elf_copy_executable(void *destination, const uint8_t *source, size_t length)
{
    volatile uint32_t *words = (volatile uint32_t *)destination;
    size_t i = 0;
    for (; length - i >= sizeof(uint32_t); i += sizeof(uint32_t)) {
        uint32_t value;
        memcpy(&value, source + i, sizeof(value));
        words[i / sizeof(uint32_t)] = value;
    }
    if (i < length) {
        uint32_t value = 0;
        memcpy(&value, source + i, length - i);
        words[i / sizeof(uint32_t)] = value;
    }
}

static void esp_elf_clear_executable(void *destination, size_t length)
{
    volatile uint32_t *words = (volatile uint32_t *)destination;
    for (size_t i = 0; i < length / sizeof(uint32_t); ++i) {
        words[i] = 0;
    }
}
#endif

/**
 * @brief Fill executable memory from a byte source using 32-bit stores only.
 *
 * Reads ELF_SOURCE_STAGE_CHUNK bytes at a time into a DRAM staging buffer
 * and forwards them through esp_elf_copy_executable(), so no sub-word
 * store ever targets instruction RAM and the source never has to be
 * resident in full. The exec allocation must cover
 * ELF_ALIGN(length, 4) bytes (the loader sizes it that way).
 *
 * @param src    - Byte source positioned anywhere; reads are offset-based
 * @param offset - Source offset of the first byte to load
 * @param dst    - Executable destination (word-aligned)
 * @param length - Number of bytes to load
 *
 * @return 0 on success or a negative errno on failure.
 */
#ifndef CONFIG_ELF_LOADER_LOAD_PSRAM
static int elf_load_exec_section(const elf_source_t *src, size_t offset,
                                 void *dst, size_t length)
{
    uint8_t *stage = esp_elf_malloc(ELF_SOURCE_STAGE_CHUNK, false);
    size_t done = 0;
    int ret;

    if (!stage) {
        return -ENOMEM;
    }

    while (done < length) {
        size_t chunk = MIN(length - done, (size_t)ELF_SOURCE_STAGE_CHUNK);

        ret = elf_source_read(src, offset + done, stage, chunk);
        if (ret) {
            esp_elf_free(stage);
            return ret;
        }

        esp_elf_copy_executable((uint8_t *)dst + done, stage, chunk);
        done += chunk;
    }

    esp_elf_free(stage);
    return 0;
}
#endif

/**
 * @brief Load ELF section.
 *
 * Reads headers and section contents on demand through @p src, so the
 * image never has to be fully resident in DRAM.
 *
 * @param elf    - ELF object pointer
 * @param src    - Byte source of the ELF image
 * @param ehdr   - ELF header (already validated and read by the caller)
 * @param shdr   - Section header table (already read by the caller)
 * @param shstrab - Section name string table (already read by the caller)
 *
 * @return ESP_OK if success or other if failed.
 */
static int esp_elf_load_section(esp_elf_t *elf, const elf_source_t *src,
                                const elf32_hdr_t *ehdr, const elf32_shdr_t *shdr,
                                const char *shstrab)
{
    int ret;
    uint32_t entry;
    uint32_t size;
    uint32_t sec_align[ELF_SECS] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    static const uint32_t data_sections[] = {
        ELF_SEC_DATA, ELF_SEC_RODATA, ELF_SEC_DRLRO, ELF_SEC_BSS,
        ELF_SEC_PREINIT, ELF_SEC_INIT, ELF_SEC_FINI
    };
    uint32_t text_size = 0;

    /* Calculate ELF image size */

    for (uint32_t i = 0; i < ehdr->shnum; i++) {
        const char *name = shstrab + shdr[i].name;
        bool reverse;
        int kind = esp_elf_lifecycle_section_kind(&shdr[i], name, &reverse);

        if (kind >= 0) {
            elf->sec[kind].v_addr = shdr[i].addr;
            elf->sec[kind].size = shdr[i].size;
            elf->sec[kind].offset = shdr[i].offset;
            sec_align[kind] = shdr[i].addralign > 1 ? shdr[i].addralign : 1;
        } else if (stype(&shdr[i], SHT_PROGBITS) && sflags(&shdr[i], SHF_ALLOC)) {
            if (sflags(&shdr[i], SHF_EXECINSTR) && !strcmp(ELF_TEXT, name)) {
                ESP_LOGD(TAG, ".text   sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_TEXT].v_addr  = shdr[i].addr;
                text_size = shdr[i].size;
                elf->sec[ELF_SEC_TEXT].size    = ELF_ALIGN(text_size, 4);
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
                sec_align[ELF_SEC_DATA] = shdr[i].addralign > 1 ? shdr[i].addralign : 1;

                ESP_LOGD(TAG, ".data   offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_DATA].offset,
                         elf->sec[ELF_SEC_DATA].size);
            } else if (!strcmp(ELF_RODATA, name)) {
                ESP_LOGD(TAG, ".rodata sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_RODATA].v_addr  = shdr[i].addr;
                elf->sec[ELF_SEC_RODATA].size    = shdr[i].size;
                elf->sec[ELF_SEC_RODATA].offset  = shdr[i].offset;
                sec_align[ELF_SEC_RODATA] = shdr[i].addralign > 1 ? shdr[i].addralign : 1;

                ESP_LOGD(TAG, ".rodata offset is 0x%lx size is 0x%x",
                         elf->sec[ELF_SEC_RODATA].offset,
                         elf->sec[ELF_SEC_RODATA].size);
            } else if (!strcmp(ELF_GOT_LOC, name)) {
                /* Xtensa l32r uses PC-relative literal addresses: these bytes
                 * must retain their offset from .text in executable memory. */
                elf->sec[ELF_SEC_GOT_LOC].v_addr = shdr[i].addr;
                elf->sec[ELF_SEC_GOT_LOC].size = shdr[i].size;
                elf->sec[ELF_SEC_GOT_LOC].offset = shdr[i].offset;
            } else if (!strcmp(ELF_DATA_REL_RO, name)) {
                ESP_LOGD(TAG, ".data.rel.ro sec addr=0x%08x size=0x%08x offset=0x%08x",
                         shdr[i].addr, shdr[i].size, shdr[i].offset);

                elf->sec[ELF_SEC_DRLRO].v_addr  = shdr[i].addr;
                elf->sec[ELF_SEC_DRLRO].size    = shdr[i].size;
                elf->sec[ELF_SEC_DRLRO].offset  = shdr[i].offset;
                sec_align[ELF_SEC_DRLRO] = shdr[i].addralign > 1 ? shdr[i].addralign : 1;

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
            sec_align[ELF_SEC_BSS] = shdr[i].addralign > 1 ? shdr[i].addralign : 1;

            ESP_LOGD(TAG, ".bss    offset is 0x%lx size is 0x%x",
                     elf->sec[ELF_SEC_BSS].offset,
                     elf->sec[ELF_SEC_BSS].size);
        }
    }

    /* No .text on image */

    if (!elf->sec[ELF_SEC_TEXT].size) {
        return -EINVAL;
    }

    for (size_t j = 0; j < sizeof(data_sections) / sizeof(data_sections[0]); j++) {
        uint32_t i = data_sections[j];
        if (sec_align[i] > 1 && (sec_align[i] & (sec_align[i] - 1))) {
            ESP_LOGE(TAG, "Invalid alignment 0x%" PRIx32 " for section %" PRIu32,
                     sec_align[i], i);
            return -EINVAL;
        }
    }

    /* Keep .got.loc at its original PC-relative offset from .text. The gap
     * can contain .rodata, but it must NOT be mapped as .text symbols. */
    uint64_t text_span = elf->sec[ELF_SEC_TEXT].size;
    const esp_elf_sec_t *got_loc = &elf->sec[ELF_SEC_GOT_LOC];
    if (got_loc->size) {
        uint64_t text_end = (uint64_t)elf->sec[ELF_SEC_TEXT].v_addr + text_size;
        if (got_loc->v_addr < text_end ||
                ((got_loc->v_addr - elf->sec[ELF_SEC_TEXT].v_addr) & 3)) {
            return -EINVAL;
        }
        uint64_t got_end = (uint64_t)got_loc->v_addr + got_loc->size;
        text_span = got_end - elf->sec[ELF_SEC_TEXT].v_addr;
        if (text_span > UINT32_MAX) {
            return -EOVERFLOW;
        }
    }
    /* The final executable byte may belong to an incomplete word. Reserve
     * a complete word for both executable stores and the PLT literal pool. */
    const uint64_t text_alloc_span = (text_span + sizeof(uint32_t) - 1) & ~(uint64_t)(sizeof(uint32_t) - 1);
    if (text_alloc_span > UINT32_MAX) {
        return -EOVERFLOW;
    }
    elf->text_alloc_size = (size_t)text_alloc_span;
    elf->ptext = esp_elf_malloc((uint32_t)text_alloc_span, true);
    if (!elf->ptext) {
        ESP_LOGE(TAG, "Failed to malloc %"PRIu32" bytes for text section",
                 (uint32_t)elf->sec[ELF_SEC_TEXT].size);
        return -ENOMEM;
    }

    uint64_t data_size = 0;
    for (size_t j = 0; j < sizeof(data_sections) / sizeof(data_sections[0]); j++) {
        uint32_t i = data_sections[j];
        if (elf->sec[i].size) {
            data_size += elf->sec[i].size;
            data_size += sec_align[i] - 1;
        }
    }
    if (data_size > UINT32_MAX) {
        ESP_LOGE(TAG, "Data sections plus alignment padding exceed allocator limit");
        esp_elf_free(elf->ptext);
        elf->ptext = NULL;
        return -EOVERFLOW;
    }

    size = (uint32_t)data_size;
    if (size) {
        elf->pdata = esp_elf_malloc(size, false);
        if (!elf->pdata) {
            ESP_LOGE(TAG, "Failed to malloc %"PRIu32" bytes for data section", size);
            esp_elf_free(elf->ptext);
            elf->ptext = NULL;
            return -ENOMEM;
        }
    }

    /* Dump ".text" from ELF to executable space memory */

    elf->sec[ELF_SEC_TEXT].addr = (uintptr_t)elf->ptext;
#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
    memset(elf->ptext, 0, elf->text_alloc_size);
    ret = elf_source_read(src, elf->sec[ELF_SEC_TEXT].offset, elf->ptext, text_size);
    if (ret) {
        goto err_free_text;
    }
#else
    esp_elf_clear_executable(elf->ptext, elf->text_alloc_size);
    ret = elf_load_exec_section(src, elf->sec[ELF_SEC_TEXT].offset, elf->ptext, text_size);
    if (ret) {
        goto err_free_text;
    }
#endif
    if (got_loc->size) {
        /* The pool is shared with .text, but the two mapped section
         * descriptors stay separate so intervening .rodata maps to DRAM. */
        elf->sec[ELF_SEC_GOT_LOC].addr =
            (uintptr_t)(elf->ptext + (got_loc->v_addr - elf->sec[ELF_SEC_TEXT].v_addr));
#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
        ret = elf_source_read(src, got_loc->offset,
                              (void *)elf->sec[ELF_SEC_GOT_LOC].addr, got_loc->size);
#else
        /* Word-staged copy into the middle of the live text block: the
         * pool offset is word-aligned so the store stays 32-bit clean. */
        ret = elf_load_exec_section(src, got_loc->offset,
                                    (void *)elf->sec[ELF_SEC_GOT_LOC].addr, got_loc->size);
#endif
        if (ret) {
            goto err_free_text;
        }
    }

#ifdef CONFIG_ELF_LOADER_SET_MMU
    if (esp_elf_arch_init_mmu(elf)) {
        esp_elf_free(elf->ptext);
        esp_elf_free(elf->pdata);
        elf->ptext = NULL;
        elf->pdata = NULL;
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
        uint32_t remaining = size;
        memset(pdata, 0, size);

        for (size_t i = 0; i < sizeof(data_sections) / sizeof(data_sections[0]); i++) {
            uint32_t sec = data_sections[i];
            uint32_t sec_size = elf->sec[sec].size;
            if (!sec_size) {
                continue;
            }

            uint32_t padding = (uint32_t)(-(uintptr_t)pdata) & (sec_align[sec] - 1);
            if (padding > remaining || sec_size > remaining - padding) {
                ESP_LOGE(TAG, "Section %" PRIu32 " exceeds allocated data buffer", sec);
                esp_elf_free(elf->pdata);
                esp_elf_free(elf->ptext);
                elf->pdata = NULL;
                elf->ptext = NULL;
                return -EOVERFLOW;
            }

            pdata += padding;
            remaining -= padding;
            elf->sec[sec].addr = (uintptr_t)pdata;
            if (sec != ELF_SEC_BSS) {
                ret = elf_source_read(src, elf->sec[sec].offset, pdata, sec_size);
                if (ret) {
                    goto err_free_text_data;
                }
            }
            pdata += sec_size;
            remaining -= sec_size;
        }
    }

    /* An entry-less shared library only exports symbols for dynamic loading. */
    if (esp_elf_is_entryless_shared_object(ehdr)) {
        elf->entry = NULL;
        return 0;
    }

    entry = ehdr->entry + elf->sec[ELF_SEC_TEXT].addr -
            elf->sec[ELF_SEC_TEXT].v_addr;

#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    elf->entry = (void *)elf_remap_text(elf, (uintptr_t)entry);
#else
    elf->entry = (void *)entry;
#endif

    return 0;

err_free_text_data:
    esp_elf_free(elf->pdata);
    elf->pdata = NULL;
err_free_text:
    esp_elf_free(elf->ptext);
    elf->ptext = NULL;
    return ret;
}

#else

/**
 * @brief Load ELF segment.
 *
 * Reads the program header table and PT_LOAD contents on demand through
 * @p src, so the image never has to be fully resident in DRAM.
 *
 * @param elf  - ELF object pointer
 * @param src  - Byte source of the ELF image
 * @param ehdr - ELF header (already read by the caller)
 *
 * @return ESP_OK if success or other if failed.
 */
static int esp_elf_load_segment(esp_elf_t *elf, const elf_source_t *src,
                                const elf32_hdr_t *ehdr)
{
    int ret;
    uint32_t size;
    bool first_segment = false;
    Elf32_Addr vaddr_s = 0;
    Elf32_Addr vaddr_e = 0;
    elf32_phdr_t *phdr = NULL;

    if (ehdr->phnum) {
        phdr = esp_elf_malloc((size_t)ehdr->phnum * sizeof(elf32_phdr_t), false);
        if (!phdr) {
            return -ENOMEM;
        }

        ret = elf_source_read(src, ehdr->phoff, phdr,
                              (size_t)ehdr->phnum * sizeof(elf32_phdr_t));
        if (ret) {
            esp_elf_free(phdr);
            return ret;
        }
    }

    for (int i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type != PT_LOAD) {
            continue;
        }

        if (phdr[i].memsz < phdr[i].filesz) {
            ESP_LOGE(TAG, "Invalid segment[%d], memsz: %d, filesz: %d",
                     i, phdr[i].memsz, phdr[i].filesz);
            esp_elf_free(phdr);
            return -EINVAL;
        }

        if (!first_segment) {
            vaddr_s = phdr[i].vaddr;
            vaddr_e = phdr[i].vaddr + phdr[i].memsz;
            first_segment = true;
            if (vaddr_e < vaddr_s) {
                ESP_LOGE(TAG, "Invalid segment[%d], vaddr: 0x%x, memsz: %d",
                         i, phdr[i].vaddr, phdr[i].memsz);
                esp_elf_free(phdr);
                return -EINVAL;
            }
        } else {
            if (phdr[i].vaddr < vaddr_e) {
                ESP_LOGE(TAG, "Invalid segment[%d], should not overlap, vaddr: 0x%x, vaddr_e: 0x%x",
                         i, phdr[i].vaddr, vaddr_e);
                esp_elf_free(phdr);
                return -EINVAL;
            }

            if ((uint64_t)phdr[i].vaddr > (uint64_t)vaddr_e + ADDR_OFFSET) {
                ESP_LOGI(TAG, "Too much padding before segment[%d], padding: %d",
                         i, phdr[i].vaddr - vaddr_e);
            }

            vaddr_e = phdr[i].vaddr + phdr[i].memsz;
            if (vaddr_e < phdr[i].vaddr) {
                ESP_LOGE(TAG, "Invalid segment[%d], address overflow, vaddr: 0x%x, vaddr_e: 0x%x",
                         i, phdr[i].vaddr, vaddr_e);
                esp_elf_free(phdr);
                return -EINVAL;
            }
        }

        ESP_LOGD(TAG, "LOAD segment[%d], vaddr: 0x%x, memsize: 0x%08x",
                 i, phdr[i].vaddr, phdr[i].memsz);
    }

    size = vaddr_e - vaddr_s;
    if (size == 0) {
        esp_elf_free(phdr);
        return -EINVAL;
    }

    elf->svaddr = vaddr_s;
    elf->psegment = esp_elf_malloc(size, true);
    if (!elf->psegment) {
        esp_elf_free(phdr);
        return -ENOMEM;
    }
    elf->segment_size = size;

    memset(elf->psegment, 0, size);

    /* Dump "PT_LOAD" from ELF to memory space */

    for (int i = 0; i < ehdr->phnum; i++) {
        if (phdr[i].type == PT_LOAD) {
            /* Subtract the virtual base before forming a host pointer. */
            uint32_t offset = phdr[i].vaddr - vaddr_s;
            ret = elf_source_read(src, phdr[i].offset,
                                  elf->psegment + offset, phdr[i].filesz);
            if (ret) {
                esp_elf_free(phdr);
                return ret;
            }
            ESP_LOGD(TAG, "Copy segment[%d], mem_addr: %p, size: 0x%08x",
                     i, (void *)(elf->psegment + offset), phdr[i].filesz);
        }
    }

    esp_elf_free(phdr);

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
    cache_ll_writeback_all(CACHE_LL_LEVEL_INT_MEM, CACHE_TYPE_DATA, CACHE_LL_ID_ALL);
#endif

    elf->entry = esp_elf_is_entryless_shared_object(ehdr) ? NULL :
                 (void *)(elf->psegment + (ehdr->entry - vaddr_s));

    return 0;
}
#endif

/**
 * @brief Override the internal symbol resolver.
 * The default resolver is based on static lists that are determined by KConfig.
 * This override allows for an arbitrary implementation.
 *
 * @param resolver the resolver function
 */
void elf_set_symbol_resolver(symbol_resolver resolver)
{
    if (!resolver) {
        ESP_LOGE(TAG, "Invalid resolver: cannot set NULL resolver");
        return;
    }

    atomic_store(&current_resolver, resolver);
}

/**
 * @brief Reset the symbol resolver to the default (static tables from KConfig).
 *
 * Equivalent to elf_set_symbol_resolver(elf_find_sym_default).
 */
void elf_reset_symbol_resolver(void)
{
    atomic_store(&current_resolver, elf_find_sym_default);
}

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
    if (!elf) {
        return 0;
    }
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    for (int i = 0; i < ELF_SECS; i++) {
        if (elf->sec[i].addr && sym >= elf->sec[i].v_addr &&
                sym - elf->sec[i].v_addr < elf->sec[i].size) {
            return sym - elf->sec[i].v_addr + elf->sec[i].addr;
        }
    }
#else
    if (elf->psegment && sym >= elf->svaddr &&
            sym - elf->svaddr < elf->segment_size) {
        return (uintptr_t)(elf->psegment + (sym - elf->svaddr));
    }
#endif
    return 0;
}

#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
/* dlsym() must return an executable function pointer. On mirrored-memory
 * targets the loaded .text address may be a data-bus alias (e.g. S3 PSRAM). */
static void *esp_elf_exported_function_address(esp_elf_t *elf, uint32_t sym_vaddr)
{
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    uintptr_t address = (uintptr_t)(elf->ptext + (sym_vaddr - elf->sec[ELF_SEC_TEXT].v_addr));
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    address = elf_remap_text(elf, address);
#endif
#else
    uintptr_t address = (uintptr_t)(elf->psegment + (sym_vaddr - elf->svaddr));
#endif
    return (void *)address;
}
#endif

/* Locate allocated constructor/destructor arrays after all relocations. */
static int esp_elf_find_lifecycle_arrays(esp_elf_t *elf, const elf32_shdr_t *shdr,
                                         uint32_t count, const char *names)
{
    for (uint32_t i = 0; i < count; ++i) {
        bool reverse;
        int kind = esp_elf_lifecycle_section_kind(&shdr[i], names + shdr[i].name, &reverse);
        if (kind < 0) {
            continue;
        }
        if ((shdr[i].size & (sizeof(uint32_t) - 1)) ||
                (shdr[i].addr & (sizeof(uint32_t) - 1))) {
            return -EINVAL;
        }

        uintptr_t first = esp_elf_map_sym(elf, shdr[i].addr);
        uintptr_t last = esp_elf_map_sym(elf, shdr[i].addr + shdr[i].size - 1);
        if (!first || last != first + shdr[i].size - 1) {
            return -EINVAL;
        }

        void (**array)(void) = (void (**)(void))first;
        uint32_t elements = shdr[i].size / sizeof(uint32_t);
        switch (kind) {
        case ELF_SEC_PREINIT:
            elf->preinit_array = array;
            elf->preinit_count = elements;
            break;
        case ELF_SEC_INIT:
            elf->init_array = array;
            elf->init_count = elements;
            elf->init_reverse = reverse;
            break;
        case ELF_SEC_FINI:
            elf->fini_array = array;
            elf->fini_count = elements;
            elf->fini_reverse = reverse;
            break;
        default:
            return -EINVAL;
        }
    }
    return 0;
}

static void esp_elf_run_lifecycle_array(void (**array)(void), uint32_t count, bool reverse)
{
    for (uint32_t i = 0; i < count; ++i) {
        void (*fn)(void) = array[reverse ? count - i - 1 : i];
        if (fn && (uintptr_t)fn != UINTPTR_MAX) { /* legacy .ctors sentinel */
            fn();
        }
    }
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

/* Section content read through an elf_source_t, kept resident only while
 * the relocation loop iterates over it. symtab/strtab repeat across the
 * relocation sections of a typical image, so caching the last section
 * index avoids re-reading them for every .rela.* table. */
typedef struct elf_section_cache {
    uint32_t index;
    uint8_t *buffer;
    size_t size;
} elf_section_cache_t;

static void elf_section_cache_reset(elf_section_cache_t *cache)
{
    if (cache->buffer) {
        esp_elf_free(cache->buffer);
    }
    cache->index = UINT32_MAX;
    cache->buffer = NULL;
    cache->size = 0;
}

static int elf_section_cache_load(elf_section_cache_t *cache, const elf_source_t *src,
                                  const elf32_shdr_t *shdr, uint32_t index)
{
    int ret;

    if (!shdr[index].size) {
        return -EINVAL;
    }

    if (cache->buffer && cache->index == index) {
        return 0;
    }

    elf_section_cache_reset(cache);

    cache->buffer = esp_elf_malloc(shdr[index].size, false);
    if (!cache->buffer) {
        ESP_LOGE(TAG, "Failed to malloc %u bytes for section %u",
                 (unsigned)shdr[index].size, (unsigned)index);
        return -ENOMEM;
    }

    ret = elf_source_read(src, shdr[index].offset, cache->buffer, shdr[index].size);
    if (ret) {
        elf_section_cache_reset(cache);
        return ret;
    }

    cache->index = index;
    cache->size = shdr[index].size;
    return 0;
}

/**
 * @brief Shared relocation core behind esp_elf_relocate and
 *        esp_elf_relocate_source.
 *
 *
 * @param elf - ELF object pointer (esp_elf_init'ed)
 * @param src - Validated byte source of the ELF image
 *
 * @return ESP_OK if success or other if failed.
 */
static int esp_elf_relocate_source_impl(esp_elf_t *elf, const elf_source_t *src)
{
    int ret;
    elf32_hdr_t ehdr;
    elf32_shdr_t *shdr = NULL;
    char *shstrab = NULL;
    size_t shdr_size;
    size_t shstrab_size;
    elf_section_cache_t symtab_cache = { .index = UINT32_MAX, .buffer = NULL, .size = 0 };
    elf_section_cache_t strtab_cache = { .index = UINT32_MAX, .buffer = NULL, .size = 0 };

    ret = elf_source_read(src, 0, &ehdr, sizeof(ehdr));
    if (ret) {
        goto out;
    }

    /* The section headers and the section name table are needed by both the
     * loader and the relocation loop, so read them once here. */

    shdr_size = (size_t)ehdr.shnum * sizeof(elf32_shdr_t);
    if (ehdr.shnum && shdr_size) {
        shdr = esp_elf_malloc(shdr_size, false);
        if (!shdr) {
            ESP_LOGE(TAG, "Failed to malloc %u bytes for section headers", (unsigned)shdr_size);
            ret = -ENOMEM;
            goto out;
        }

        ret = elf_source_read(src, ehdr.shoff, shdr, shdr_size);
        if (ret) {
            goto out;
        }

        shstrab_size = (ehdr.shstrndx < ehdr.shnum) ? shdr[ehdr.shstrndx].size : 0;
        if (shstrab_size) {
            shstrab = esp_elf_malloc(shstrab_size, false);
            if (!shstrab) {
                ESP_LOGE(TAG, "Failed to malloc %u bytes for section names", (unsigned)shstrab_size);
                ret = -ENOMEM;
                goto out;
            }

            ret = elf_source_read(src, shdr[ehdr.shstrndx].offset, shstrab, shstrab_size);
            if (ret) {
                goto out;
            }
        }
    }

    /* Load section or segment to memory space */

#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    ret = esp_elf_load_section(elf, src, &ehdr, shdr, shstrab);
#else
    ret = esp_elf_load_segment(elf, src, &ehdr);
#endif

    if (ret) {
        ESP_LOGE(TAG, "Error to load elf file, ret=%d", ret);
        goto out;
    }

    ESP_LOGI(TAG, "elf->entry=%p", elf->entry);

    /* Relocation section data */

    for (uint32_t i = 0; i < ehdr.shnum; i++) {
        if (stype(&shdr[i], SHT_RELA)) {
            uint32_t nr_reloc;
            uint8_t *rela_buf;
            const elf32_rela_t *rela;
            const elf32_sym_t *symtab;
            const char *strtab;

            nr_reloc = shdr[i].size / sizeof(elf32_rela_t);
            if (!nr_reloc) {
                continue;
            }

            ret = elf_section_cache_load(&symtab_cache, src, shdr, shdr[i].link);
            if (ret) {
                goto out;
            }

            ret = elf_section_cache_load(&strtab_cache, src, shdr, shdr[shdr[i].link].link);
            if (ret) {
                goto out;
            }

            symtab = (const elf32_sym_t *)symtab_cache.buffer;
            strtab = (const char *)strtab_cache.buffer;

            rela_buf = esp_elf_malloc(shdr[i].size, false);
            if (!rela_buf) {
                ESP_LOGE(TAG, "Failed to malloc %"PRIu32" bytes for relocation", shdr[i].size);
                ret = -ENOMEM;
                goto out;
            }

            ret = elf_source_read(src, shdr[i].offset, rela_buf, shdr[i].size);
            if (ret) {
                esp_elf_free(rela_buf);
                goto out;
            }

            rela = (const elf32_rela_t *)rela_buf;

            ESP_LOGD(TAG, "Section %s has %d symbol tables",
                     shstrab ? (shstrab + shdr[i].name) : "?", (int)nr_reloc);

            for (uint32_t j = 0; j < nr_reloc; j++) {
                int type;
                uintptr_t addr = 0;

                const elf32_sym_t *sym = &symtab[ELF_R_SYM(rela[j].info)];

                type = ELF_R_TYPE(rela[j].info);
                if (type == STT_COMMON || type == STT_OBJECT || type == STT_SECTION) {
                    const char *comm_name = strtab + sym->name;

                    /* A local definition, including an unnamed STT_SECTION,
                     * must never be preempted by a same-name host export.
                     * Named global/weak definitions retain host preemption. */
                    if (sym->shndx != SHN_UNDEF &&
                            (ELF_ST_BIND(sym->info) == STB_LOCAL || !comm_name[0])) {
                        addr = esp_elf_map_sym(elf, sym->value);
                    } else if (comm_name[0]) {
                        addr = elf_find_sym(comm_name);
                        if (!addr && sym->shndx != SHN_UNDEF) {
                            addr = esp_elf_map_sym(elf, sym->value);
                        }
                    }

                    if (!addr && (comm_name[0] || sym->shndx != SHN_UNDEF) &&
                            !(sym->shndx == SHN_UNDEF && ELF_ST_BIND(sym->info) == STB_WEAK)) {
                        ESP_LOGE(TAG, "Can't find common %s", comm_name);
                        esp_elf_free(rela_buf);
                        ret = -ENOSYS;
                        goto out;
                    }
                    ESP_LOGD(TAG, "Find common %s addr=%x", comm_name, addr);
                } else if (type == STT_FILE) {
                    const char *func_name = strtab + sym->name;

                    if (sym->value) {
                        addr = esp_elf_map_sym(elf, sym->value);
                    } else {
                        addr = elf_find_sym(func_name);
                    }

                    if (!addr && sym->shndx != SHN_UNDEF) {
                        addr = esp_elf_map_sym(elf, sym->value);
                    }
                    if (!addr && !(sym->shndx == SHN_UNDEF &&
                                   ELF_ST_BIND(sym->info) == STB_WEAK)) {
                        ESP_LOGE(TAG, "Can't find symbol %s", func_name);
                        esp_elf_free(rela_buf);
                        ret = -ENOSYS;
                        goto out;
                    }

                    ESP_LOGD(TAG, "Find function %s addr=%x", func_name, addr);
                }

                ret = esp_elf_arch_relocate(elf, &rela[j], sym, addr);
                if (ret) {
                    ESP_LOGE(TAG, "Failed to relocate symbol, ret=%d", ret);
                    esp_elf_free(rela_buf);
                    goto out;
                }
            }

            esp_elf_free(rela_buf);
        }
#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
        else if (shstrab && !strcmp(shstrab + shdr[i].name, ELF_DYNSYM)) {
            int j;
            uint32_t len;
            uint16_t num = 0;
            const elf32_sym_t *symtab;
            const char *strtab;

            ret = elf_section_cache_load(&symtab_cache, src, shdr, i);
            if (ret) {
                goto out;
            }

            ret = elf_section_cache_load(&strtab_cache, src, shdr, shdr[i].link);
            if (ret) {
                goto out;
            }

            symtab = (const elf32_sym_t *)symtab_cache.buffer;
            strtab = (const char *)strtab_cache.buffer;

            elf->num = 0;
            for (j = 0; j < shdr[i].size / sizeof(elf32_sym_t); j++) {
                if ((ELF_ST_BIND(symtab[j].info) == STB_GLOBAL) &&
                        (ELF_ST_TYPE(symtab[j].info) == STT_FUNC) &&
                        symtab[j].shndx != SHN_UNDEF) {
                    elf->num++;
                }
            }

            if (elf->num) {
                elf->symtab = (esp_symtab_t *)esp_elf_malloc(elf->num * sizeof(esp_symtab_t), false);
                if (!elf->symtab) {
                    ESP_LOGE(TAG, "Failed to malloc for symbol table");
                    ret = -ENOMEM;
                    goto out;
                }

                memset(elf->symtab, 0, elf->num * sizeof(esp_symtab_t));
            }

            for (j = 0; j < shdr[i].size / sizeof(elf32_sym_t); j++) {
                if ((ELF_ST_BIND(symtab[j].info) == STB_GLOBAL) &&
                        (ELF_ST_TYPE(symtab[j].info) == STT_FUNC) &&
                        symtab[j].shndx != SHN_UNDEF) {
                    len = strlen((const char *)(strtab + symtab[j].name)) + 1;
                    elf->symtab[num].addr = esp_elf_exported_function_address(elf, symtab[j].value);
                    elf->symtab[num].name = esp_elf_malloc(len, false);
                    if (!elf->symtab[num].name) {
                        ESP_LOGE(TAG, "Failed to malloc for symbol table name");
                        elf->num = num;
                        ret = -ENOMEM;
                        goto out;
                    }

                    memset((void *)elf->symtab[num].name, 0, len);
                    memcpy((void *)elf->symtab[num].name, strtab + symtab[j].name, len);
                    ESP_LOGI(TAG, "elf->symtab[%d], func: %s", num, strtab + symtab[j].name);
                    num++;
                }
            }
        }
#endif
    }

    ret = esp_elf_find_lifecycle_arrays(elf, shdr, ehdr.shnum, shstrab);
    if (ret) {
        goto out;
    }

#ifdef CONFIG_ELF_LOADER_LOAD_PSRAM
    ret = esp_elf_arch_flush(elf);
    if (ret) {
        ESP_LOGE(TAG, "Failed to synchronize ELF cache, ret=%d", ret);
        goto out;
    }
#endif

out:
    if (src && src->close) {
        src->close(src->ctx);
    }
    elf_section_cache_reset(&symtab_cache);
    elf_section_cache_reset(&strtab_cache);
    esp_elf_free(shdr);
    esp_elf_free(shstrab);

    return ret;
}

/* In-memory elf_source_t over an already-validated whole buffer. */
typedef struct {
    const uint8_t *buffer;
    size_t         size;
} elf_mem_source_t;

static size_t elf_mem_source_size(void *ctx)
{
    return ((const elf_mem_source_t *)ctx)->size;
}

static int elf_mem_source_read(void *ctx, size_t offset, void *buf, size_t len)
{
    const elf_mem_source_t *mem = (const elf_mem_source_t *)ctx;

    if (offset > mem->size || len > mem->size - offset) {
        return -EINVAL;
    }

    memcpy(buf, mem->buffer + offset, len);

    return 0;
}

/**
 * @brief Decode and relocate ELF data.
 *
 * @param elf - ELF object pointer
 * @param pbuf - ELF data buffer
 * @param size - Length of the ELF data buffer in bytes
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_relocate(esp_elf_t *elf, const uint8_t *pbuf, size_t size)
{
    if (!elf || !pbuf) {
        return -EINVAL;
    }

    /* Reject malformed images before dereferencing any ELF-controlled offset.
     * The mem source bounds its reads by the same size, so the core skips
     * re-running structural validation for this path. */
    int ret = esp_elf_validate(pbuf, size);
    if (ret) {
        ESP_LOGE(TAG, "Invalid ELF image, ret=%d", ret);
        return ret;
    }

    elf_mem_source_t mem = { .buffer = pbuf, .size = size };
    const elf_source_t src = {
        .size  = elf_mem_source_size,
        .read  = elf_mem_source_read,
        .close = NULL,
        .ctx   = &mem,
    };

    return esp_elf_relocate_source_impl(elf, &src);
}

/**
 * @brief Decode and relocate ELF data read on demand from a byte source.
 *
 * The loader reads headers, sections and relocation tables on demand through
 * @p src without holding the entire image in DRAM. The source must support
 * random access by offset.
 *
 * @note The loader calls src->close() exactly once before returning, whether
 *       it succeeds or fails, except when src or src->read is NULL. The
 *       caller must not close the source itself.
 * @note The image must not change between validation and relocation or while
 *       reads are in progress; bounds are checked during validation and
 *       trusted on subsequent reads.
 * @note Temporary buffers for large tables (e.g. .symtab, .rela.*) may still
 *       be megabytes; a buffered source is strongly recommended for SD.
 *
 * @param elf - ELF object pointer (esp_elf_init'ed)
 * @param src - Byte source of the ELF image
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_relocate_source(esp_elf_t *elf, const elf_source_t *src)
{
    if (!elf || !src || !src->read) {
        return -EINVAL;
    }

    /* Same strictness as the whole-buffer path: the checks are offset-based,
     * so they bound every read through the source before the loader
     * dereferences any ELF-controlled value. */
    int ret = esp_elf_validate_source(src);
    if (ret) {
        ESP_LOGE(TAG, "Invalid ELF image, ret=%d", ret);
        if (src && src->close) {
            src->close(src->ctx);
        }
        return ret;
    }

    return esp_elf_relocate_source_impl(elf, src);
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

    if (!elf->init_done) {
        elf->init_done = true;
        esp_elf_run_lifecycle_array(elf->preinit_array, elf->preinit_count, false);
        esp_elf_run_lifecycle_array(elf->init_array, elf->init_count, elf->init_reverse);
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
 *
 * @note This function frees all resources allocated by esp_elf_relocate() and
 *       resets the structure to its initial state (same as after esp_elf_init()).
 */
void esp_elf_deinit(esp_elf_t *elf)
{
    if (!elf) {
        return;
    }

    if (elf->init_done) {
        esp_elf_run_lifecycle_array(elf->fini_array, elf->fini_count, elf->fini_reverse);
    }

#ifdef CONFIG_ELF_LOADER_SET_MMU
    /* Unmap executable PSRAM before returning its pages to the allocator.
     * Otherwise the I-bus temporarily aliases memory owned by another user. */
    if (elf->mmu_num) {
        esp_elf_arch_deinit_mmu(elf);
    }
#endif

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
        esp_elf_free(elf->psegment);
        elf->psegment = NULL;
    }
#endif

#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    /* On a failed first symbol-name allocation, num becomes zero even though
     * the dynamic symbol table allocation is still owned by this ELF. */
    if (elf->symtab) {
        for (int i = 0; i < elf->num; i++) {
            if (elf->symtab[i].name) {
                esp_elf_free(elf->symtab[i].name);
            }
        }

        esp_elf_free(elf->symtab);
        elf->symtab = NULL;
    }

    elf->num = 0;
#endif

    /* Reset structure to initial state (same as esp_elf_init) */
    memset(elf, 0, sizeof(esp_elf_t));
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

uintptr_t esp_elf_runtime_base(esp_elf_t *elf)
{
    if (!elf) {
        return 0;
    }
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
    const esp_elf_sec_t *text = &elf->sec[ELF_SEC_TEXT];
    if (!text->addr || !text->size) {
        return 0;
    }
#ifdef CONFIG_ELF_LOADER_CACHE_OFFSET
    return elf_remap_text(elf, text->addr);
#else
    return text->addr;
#endif
#else
    return elf->psegment && elf->segment_size ? (uintptr_t)elf->psegment : 0;
#endif
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
        "text", "bss", "data", "rodata", "data.rel.ro", "got.loc",
        "preinit_array", "init_array", "fini_array"
    };

    for (int i = 0; i < ELF_SECS; i++) {
        ESP_LOGI(TAG, "%s:   0x%08x size 0x%08x", sec_names[i], elf->sec[i].addr, elf->sec[i].size);
    }

    ESP_LOGI(TAG, "entry:  %p", elf->entry);

    /* Print the address mapping needed to decode PCs inside loaded code.
     * On mirrored buses, .text addr is not necessarily an executable alias. */
    uintptr_t base = esp_elf_runtime_base(elf);
    if (base) {
#if CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR
        ESP_LOGI(TAG, "ELF debug: runtime_base=0x%" PRIxPTR " ELF_text_vaddr=0x%" PRIxPTR,
                 base, elf->sec[ELF_SEC_TEXT].v_addr);
#else
        ESP_LOGI(TAG, "ELF debug: runtime_base=0x%" PRIxPTR " ELF_segment_vaddr=0x%" PRIx32,
                 base, elf->svaddr);
#endif
    }
}

/**
 * @brief Register symbol table to global symbol tables array.
 *
 * @param symbol_table - Pointer to symbol table structure (array of esp_elfsym terminated by ESP_ELFSYM_END)
 *
 * @return 0 if success, -EINVAL if symbol_table is NULL, -EEXIST if already registered, -ENOMEM if no space.
 *
 * @note This function is not thread-safe. External synchronization must be used if calling
 *       this function concurrently from multiple threads.
 */
int esp_elf_register_symbol(esp_elf_symbol_table_t *symbol_table)
{
    if (!symbol_table) {
        return -EINVAL;
    }

    for (int i = 0; i < SYMBOL_TABLES_NO; i++) {
        if (g_symbol_tables[i] == symbol_table) {
            return -EEXIST;
        } else if (g_symbol_tables[i] == NULL) {
            g_symbol_tables[i] = symbol_table;
            return 0;
        }
    }

    return -ENOMEM;
}

/**
 * @brief Unregister symbol table from global symbol tables array.
 *
 * @param symbol_table - Pointer to symbol table structure to remove
 *
 * @return 0 if success, -EINVAL if symbol_table is NULL or symbol table not found.
 *
 * @note This function is not thread-safe. External synchronization must be used if calling
 *       this function concurrently from multiple threads.
 */
int esp_elf_unregister_symbol(esp_elf_symbol_table_t *symbol_table)
{
    if (!symbol_table) {
        return -EINVAL;
    }

    for (int i = 0; i < SYMBOL_TABLES_NO; i++) {
        if (g_symbol_tables[i] == symbol_table) {
            g_symbol_tables[i] = NULL;
            return 0;
        }
    }

    return -EINVAL;
}

/**
 * @brief Find symbol address by symbol name in registered tables.
 *
 * @param sym_name - Symbol name string to search
 *
 * @return Symbol address if found, 0 if not found.
 * @note Search order is registration order (earliest registered first).
 */
uintptr_t esp_elf_find_symbol(const char *sym_name)
{
    if (!sym_name) {
        return 0;
    }

    esp_elf_symbol_table_t *syms;
    for (int i = 0; i < SYMBOL_TABLES_NO; i++) {
        if (g_symbol_tables[i]) {
            syms = g_symbol_tables[i];
            while (syms->name) {
                if (!strcmp(syms->name, sym_name)) {
                    return (uintptr_t)syms->sym;
                }

                syms++;
            }
        }
    }

    return 0;
}
