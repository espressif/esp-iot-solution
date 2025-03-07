/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "esp_dlfcn.h"

#include "private/esp_dlmod.h"
#include "private/elf_platform.h"

static const char *TAG = "DLFCN";

/**
 * @brief Load and register a module from a filesystem path.
 *
 * @param path - Filesystem path to the ELF binary
 *
 * @return Module handle on success, NULL on failure.
 */
void *dlinsmod(const char *path)
{
    if (!path) {
        ESP_LOGE(TAG, "Invalid NULL path");
        return NULL;
    }

    char filename[FILE_NAME_MAX] = {0};
    if (!dlmod_getname(path, filename, FILE_NAME_MAX)) {
        ESP_LOGE(TAG, "Failed to get filename");
        return NULL;
    }

    /* Then install the file using the basename of the file as the module name. */

    return dlmod_insert(path, filename);
}

/**
 * @brief Unload a module by its filesystem path.
 *
 * @param path - Filesystem path used during module loading
 *
 * @return 0 on success, -1 on failure (invalid path or not loaded).
 */
int dlrmmod(const char *path)
{
    if (!path) {
        ESP_LOGE(TAG, "Invalid NULL path");
        return -1;
    }

    char filename[FILE_NAME_MAX] = {0};
    if (!dlmod_getname(path, filename, FILE_NAME_MAX)) {
        ESP_LOGE(TAG, "Failed to get filename");
        return -1;
    }

    return dlmod_remove(dlmod_gethandle(filename));
}

/**
 * @brief Dynamic loader compatibility interface - Load a shared object.
 *
 * @param path  - Filesystem path to the ELF binary
 * @param flags - (Unused) Compatibility parameter
 *
 * @return Module handle on success, NULL on failure.
 */
void *dlopen(const char *path, int flags)
{
    if (!path) {
        ESP_LOGE(TAG, "Invalid NULL path");
        return NULL;
    }

    char filename[FILE_NAME_MAX] = {0};
    if (!dlmod_getname(path, filename, FILE_NAME_MAX)) {
        ESP_LOGE(TAG, "Failed to get filename");
        return NULL;
    }

    return dlmod_insert(path, filename);
}

/**
 * @brief Dynamic loader compatibility interface - Close a module handle.
 *
 * @param handle - Module handle to close
 *
 * @return 0 on success, -1 on invalid handle.
 */
int dlclose(void *handle)
{
    return dlmod_remove(handle);
}

/**
 * @brief Dynamic loader compatibility interface - Look up symbol address.
 *
 * @param handle - Module handle from dlopen()
 * @param symbol - Symbol name to locate
 *
 * @return Memory address of the symbol if found, NULL otherwise.
 */
void *dlsym(void *handle, const char *symbol)
{
    struct dlmod_slist_t *mod = (struct dlmod_slist_t *)handle;

    if (mod && mod->elf && symbol) {
        for (int i = 0; i < mod->elf->num; i++) {
            if (strcmp(mod->elf->symtab[i].name, symbol) == 0) {
                return mod->elf->symtab[i].addr;
            }
        }
    }

    ESP_LOGW(TAG, "'%s' not found in the symbol table lists", symbol);
    return NULL;
}

/**
 * @brief Display system module/symbol information.
 *
 * @param type - Listing type (LIST_MODULE for modules, LIST_SYMBOL for symbols)
 */
void dllist(dl_list_t type)
{
    if (type == LIST_MODULE) {
        dlmod_listname();
    } else if (type == LIST_SYMBOL) {
        dlmod_listsymbol();
    }
}
