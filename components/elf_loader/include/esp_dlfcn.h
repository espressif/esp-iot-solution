/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The MODE argument to `dlopen' contains one of the following: */

#define RTLD_LAZY           0x00001 /*!< Lazy function call binding. */
#define RTLD_NOW            0x00002 /*!< Immediate function call binding. */
#define RTLD_BINDING_MASK   0x00003 /*!< Mask of binding time value. */
#define RTLD_NOLOAD         0x00004 /*!< Do not load the object. */
#define RTLD_DEEPBIND       0x00008 /*!< Use deep binding. */

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
 * @param file - Filesystem path to the ELF binary
 * @param mode - Mode flags (RTLD_LAZY, RTLD_NOW, etc.)
 *
 * @return Module handle on success, NULL on failure.
 */
void *dlopen(const char *file, int mode);

/**
 * @brief Dynamic loader compatibility interface - Look up symbol address.
 *
 * @param handle - Module handle from dlopen()
 * @param name - Symbol name to locate
 *
 * @return Memory address of the symbol if found, NULL otherwise.
 */
void *dlsym(void *handle, const char *name);

/**
 * @brief Dynamic loader compatibility interface - Close a module handle.
 *
 * @param handle - Module handle to close
 *
 * @return 0 on success, -1 on invalid handle.
 */
int dlclose(void *handle);

/**
 * @brief Dynamic loader compatibility interface - Get error message.
 *
 * @note Thread safety: The returned pointer points to a static buffer that
 *       may be overwritten by subsequent calls to dl* functions from any
 *       thread. Callers should copy the string immediately if needed.
 *
 * @return Pointer to error message string, or NULL if no error occurred.
 */
const char *dlerror(void);

/**
 * @brief Display system module/symbol information.
 *
 * @param type - Listing type (LIST_MODULE for modules, LIST_SYMBOL for symbols)
 */
void dllist(dl_list_t type);

#ifdef __cplusplus
}
#endif
