/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __DLFCN_H__
#define __DLFCN_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY   (0 << 0)
#define RTLD_NOW    (1 << 0)
#define RTLD_GLOBAL (1 << 1)
#define RTLD_LOCAL  (1 << 2)

/**
 * @brief Enum for symbol table list type
 *
 */
typedef enum {
    LIST_MODULE,
    LIST_SYMBOL,
} dl_list_t;

/**
 * @brief Dynamic loader compatibility interface - Load a shared object.
 *
 * @param path  - Filesystem path to the ELF binary
 * @param flags - (Unused) Compatibility parameter
 *
 * @return Module handle on success, NULL on failure.
 */
void *dlopen(const char *filename, int flag);

/**
 * @brief Dynamic loader compatibility interface - Look up symbol address.
 *
 * @param handle - Module handle from dlopen()
 * @param symbol - Symbol name to locate
 *
 * @return Memory address of the symbol if found, NULL otherwise.
 */
void *dlsym(void *handle, const char *symbol);

/**
 * @brief Dynamic loader compatibility interface - Close a module handle.
 *
 * @param handle - Module handle to close
 *
 * @return 0 on success, -1 on invalid handle.
 */
int dlclose(void *handle);

/**
 * @brief Load and register a module from a filesystem path.
 *
 * @param path - Filesystem path to the ELF binary
 *
 * @return Module handle on success, NULL on failure.
 */
void *dlinsmod(const char *path);

/**
 * @brief Unload a module by its filesystem path.
 *
 * @param path - Filesystem path used during module loading
 *
 * @return 0 on success, -1 on failure (invalid path or not loaded).
 */
int dlrmmod(const char *path);

/**
 * @brief Display system module/symbol information.
 *
 * @param type - Listing type (LIST_MODULE for modules, LIST_SYMBOL for symbols)
 */
void dllist(dl_list_t type);

#ifdef __cplusplus
}
#endif
#endif
