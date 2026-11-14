/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "private/elf_types.h"
#include "private/elf_symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct elf_file {
    uint8_t *payload;
    size_t size;
} elf_file_t;

/**
 * @brief Random-access byte source for streaming ELF relocation.
 *
 * The loader reads headers, sections and relocation tables on demand
 * through @p read, so the whole image never has to be resident in DRAM.
 * @p size is required so structural validation can bound every read the
 * same way the whole-buffer loader does.
 */
typedef struct elf_source {
    /**
     * @brief Total image length in bytes.
     *
     * @return Image size; every read offset and length is checked
     *         against it before the loader dereferences anything.
     */
    size_t (*size)(void *ctx);

    /**
     * @brief Read @p len bytes starting at @p off into @p buf.
     *
     * @return 0 on success or a negative errno on failure.
     */
    int (*read)(void *ctx, size_t off, void *buf, size_t len);

    /**
     * @brief Optional cleanup callback invoked after relocation.
     *
     * @note May be NULL when the source needs no teardown.
     */
    void (*close)(void *ctx);

    void *ctx;                   /*!< opaque context passed to the callbacks */
} elf_source_t;

/**
 * @brief Symbol table type
 *
 * A symbol table is an array of esp_elfsym structures terminated with ESP_ELFSYM_END.
 */
typedef const struct esp_elfsym esp_elf_symbol_table_t;

/** @brief Compiled symbol registry layout version. */
#define ESP_ELF_SYMBOL_REGISTRY_VERSION 1

/** @brief Read-only compiled tables in default resolver order. */
typedef struct esp_elf_symbol_registry {
    uint32_t version;                          /*!< registry layout version */
    uint32_t table_count;                      /*!< table count excluding NULL terminator */
    esp_elf_symbol_table_t *const *tables;     /*!< NULL-terminated array of table pointers */
} esp_elf_symbol_registry_t;

/**
 * @brief Compiled symbol registry for runtime and firmware inspection.
 *
 * @note Contains enabled libc, ESP-IDF and customer tables, in that order.
 *       Entries use ESP_ELFSYM_END; the first matching name wins.
 *       Runtime registrations, shared objects and custom resolvers are excluded.
 *       Static storage; valid for the firmware lifetime.
 */
extern const esp_elf_symbol_registry_t g_esp_elf_static_symbol_registry;

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
int esp_elf_open(elf_file_t *file, const char *name);

/**
 * @brief Close ELF file and release associated resources.
 *
 * @param file - Pointer to opened elf_file_t structure
 *
 * @note Releases memory allocated by esp_elf_open() for payload data
 * @note Should be called paired with esp_elf_open() to prevent memory leaks
 * @note If file is NULL, this function does nothing (null-safe)
 */
void esp_elf_close(elf_file_t *file);

/**
 * @brief Map symbol's address of ELF to physic space.
 *
 * @param elf - ELF object pointer
 * @param sym - ELF symbol address
 *
 * @return Mapped physic address.
 */
uintptr_t esp_elf_map_sym(esp_elf_t *elf, uintptr_t sym);

/**
 * @brief Get the runtime executable address corresponding to the ELF load base.
 *
 * For mirrored Xtensa targets this is the executable .text bus alias;
 * for RISC-V it is the allocated lowest PT_LOAD address. Returns zero for
 * a module that has not been loaded.
 *
 * Use together with the matching unstripped ELF to symbolize crash PCs.
 */
uintptr_t esp_elf_runtime_base(esp_elf_t *elf);

/**
 * @brief Initialize ELF object.
 *
 * @param elf - ELF object pointer
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_init(esp_elf_t *elf);

/**
 * @brief Decode and relocate ELF data.
 *
 * @param elf - ELF object pointer
 * @param pbuf - ELF data buffer
 * @param size - Exact number of accessible bytes in pbuf
 *
 * @note The buffer size is mandatory so ELF header, section, symbol and
 *       relocation references can be checked before loading the image.
 *       The input buffer must also be 4-byte aligned.
 *       When dynamic loading is enabled, ET_DYN shared objects with no entry
 *       point are supported, but they cannot be passed to esp_elf_request().
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_relocate(esp_elf_t *elf, const uint8_t *pbuf, size_t size);

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
int esp_elf_relocate_source(esp_elf_t *elf, const elf_source_t *src);

/**
 * @brief Request running relocated ELF function.
 *
 * @param elf  - ELF object pointer
 * @param opt  - Request options
 * @param argc - Arguments number
 * @param argv - Arguments value array
 *
 * @return ESP_OK if success or other if failed.
 *
 * @note Returns -EINVAL for NULL, missing entry or finalized images;
 *       -EBUSY for reentrant lifecycle calls.
 * @note Externally serialize calls on the same object. Entry and lifecycle
 *       callbacks must return normally; non-local exits are unsupported.
 */
int esp_elf_request(esp_elf_t *elf, int opt, int argc, char *argv[]);

/**
 * @brief Finalize an ELF image without releasing its memory or MMU mappings.
 *
 * @param elf - Initialized ELF object. May be called after failed relocation;
 *              callbacks are skipped if startup never ran.
 *
 * @return ESP_OK if success or other if failed.
 *
 * @note Returns -EINVAL for NULL; -EBUSY during entry or lifecycle callbacks.
 * @note Runs callbacks once, in the calling task, only if startup ran.
 *       Repeated calls succeed. Finalized images cannot execute again.
 *       Call esp_elf_deinit() after host-managed cleanup to release the image.
 * @note Quiesce module tasks and externally serialize object access.
 *       Callbacks must return normally. Does not invoke __cxa_finalize.
 */
int esp_elf_finalize(esp_elf_t *elf);

/**
 * @brief Deinitialize ELF object.
 *
 * @param elf - ELF object pointer
 *
 * @return None
 *
 * @note Finalizes, frees and resets the object. NULL is accepted. Reentrant
 *       calls log an error and retain the image; retry after execution returns.
 *       Quiesce module tasks and externally serialize object access.
 */
void esp_elf_deinit(esp_elf_t *elf);

/**
 * @brief Print header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_ehdr(const uint8_t *pbuf);

/**
 * @brief Print program header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_phdr(const uint8_t *pbuf);

/**
 * @brief Print section header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_shdr(const uint8_t *pbuf);

/**
 * @brief Print section information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_sec(esp_elf_t *elf);

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
int esp_elf_register_symbol(esp_elf_symbol_table_t *symbol_table);

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
int esp_elf_unregister_symbol(esp_elf_symbol_table_t *symbol_table);

/**
 * @brief Find symbol address by symbol name in registered tables.
 *
 * @param sym_name - Symbol name string to search
 *
 * @return Symbol address if found, 0 if not found.
 * @note Searches slots from lowest to highest. Registration reuses the first
 *       empty slot; unregistering a table does not reorder other slots.
 */
uintptr_t esp_elf_find_symbol(const char *sym_name);

#ifdef __cplusplus
}
#endif
