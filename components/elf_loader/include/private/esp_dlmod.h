/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/queue.h>

#include "private/elf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_NAME_MAX 64
#define DLMOD_HANDLE_MAGIC 0x444C4D4FUL

/**
 * @brief Structure for recording the dynamic load shared objects
 *
 */
struct dlmod_slist_t {
    uint32_t magic;
    char name[FILE_NAME_MAX];
    esp_elf_t *elf;
    SLIST_ENTRY(dlmod_slist_t) next;
};

/**
 * @brief Extract the base filename from a given path, removing directory and extension.
 *
 * @param path     - Full path to the file
 * @param filename - Buffer to store the extracted filename
 * @param len      - Length of the filename buffer
 *
 * @return Pointer to the filename buffer on success, NULL on invalid parameters or truncation.
 */
char *dlmod_getname(const char *path, char *filename, size_t len);

/**
 * @brief Retrieve a module handle by name from the global module list.
 *
 * @param name - Name of the module to find (without .so extension)
 *
 * @return Pointer to the module handle if found, NULL if not found or invalid parameters.
 */
void *dlmod_gethandle(const char *name);

/**
 * @brief Relocate and load an ELF file from the specified path.
 *
 * @param path - Filesystem path to the ELF binary
 *
 * @return Pointer to the relocated ELF structure, NULL on relocation failure.
 */
void *dlmod_relocate(const char *path);

/**
 * @brief Insert a new module into the global module list.
 *
 * @param path - Filesystem path to the ELF binary
 * @param name - Module name to register (should match filename base)
 *
 * @return Pointer to the new module entry, NULL on failure (existing entry or relocation error).
 */
void *dlmod_insert(const char *path, const char *name);

/**
 * @brief Remove a module from the global module list and free resources.
 *
 * @param handle - Module handle to remove
 *
 * @return 0 on successful removal, -1 if handle not found.
 */
int dlmod_remove(void *handle);

/**
 * @brief Validate a module handle returned by dlmod_insert() or dlmod_relocate().
 *
 * @param handle - Module handle to validate
 *
 * @return true if the handle is a valid module entry, false otherwise.
 */
bool dlmod_validate_handle(void *handle);

/**
 * @brief Find a symbol address by name across all loaded modules.
 *
 * @param sym_name - Symbol name to search for
 *
 * @return Memory address of the symbol if found, NULL otherwise.
 */
void *dlmod_getaddr(const char *sym_name);

/**
 * @brief List all loaded module names in the system.
 */
void dlmod_listname(void);

/**
 * @brief List all symbols from all loaded modules with their addresses.
 */
void dlmod_listsymbol(void);

#ifdef __cplusplus
}
#endif
