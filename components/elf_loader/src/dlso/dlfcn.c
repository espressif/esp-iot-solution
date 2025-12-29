/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_dlfcn.h"

#include "private/esp_dlmod.h"
#include "private/elf_platform.h"

static const char *TAG = "DLFCN";

#define DL_ERROR_MSG_MAX_LEN 128
static char g_dlerror_msg[DL_ERROR_MSG_MAX_LEN] = {0};
static bool g_dlerror_set = false;

/**
 * @brief Initialize the mutex for thread-safe error state access
 * @note Uses static allocation to avoid race conditions
 */
static StaticSemaphore_t g_dlerror_mutex_buffer;
static SemaphoreHandle_t g_dlerror_mutex = NULL;

__attribute__((constructor))
static void dlerror_mutex_init(void)
{
    if (g_dlerror_mutex == NULL) {
        g_dlerror_mutex = xSemaphoreCreateMutexStatic(&g_dlerror_mutex_buffer);
        if (g_dlerror_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex for dlerror");
        }
    }
}

/**
 * @brief Set error message for dlerror()
 */
static void dlerror_set(const char *msg)
{
    dlerror_mutex_init();
    if (g_dlerror_mutex == NULL) {
        // If mutex creation failed, still try to set error (best effort)
        if (msg) {
            strncpy(g_dlerror_msg, msg, DL_ERROR_MSG_MAX_LEN - 1);
            g_dlerror_msg[DL_ERROR_MSG_MAX_LEN - 1] = '\0';
            g_dlerror_set = true;
        } else {
            g_dlerror_set = false;
            g_dlerror_msg[0] = '\0';
        }
        return;
    }

    if (xSemaphoreTake(g_dlerror_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take dlerror mutex");
        return;
    }

    if (msg) {
        strncpy(g_dlerror_msg, msg, DL_ERROR_MSG_MAX_LEN - 1);
        g_dlerror_msg[DL_ERROR_MSG_MAX_LEN - 1] = '\0';
        g_dlerror_set = true;
    } else {
        g_dlerror_set = false;
        g_dlerror_msg[0] = '\0';
    }

    xSemaphoreGive(g_dlerror_mutex);
}

/**
 * @brief Dynamic loader compatibility interface - Get error message.
 *
 * @note Thread safety: The returned pointer points to a static buffer that
 *       may be overwritten by subsequent calls to dl* functions from any
 *       thread. Callers should copy the string immediately if needed.
 *
 * @return Pointer to error message string, or NULL if no error occurred.
 */
const char *dlerror(void)
{
    dlerror_mutex_init();
    if (g_dlerror_mutex == NULL) {
        // If mutex creation failed, still try to read error (best effort)
        if (g_dlerror_set) {
            g_dlerror_set = false;
            return g_dlerror_msg;
        }
        return NULL;
    }

    if (xSemaphoreTake(g_dlerror_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take dlerror mutex");
        return NULL;
    }

    const char *result = NULL;
    if (g_dlerror_set) {
        g_dlerror_set = false;
        result = g_dlerror_msg;
    }

    xSemaphoreGive(g_dlerror_mutex);
    return result;
}

/**
 * @brief Dynamic loader compatibility interface - Load a shared object.
 *
 * @param file - Filesystem path to the ELF binary
 * @param mode - Mode flags (RTLD_LAZY, RTLD_NOW, etc.)
 *
 * @return Module handle on success, NULL on failure.
 */
void *dlopen(const char *file, int mode)
{
    (void)mode; // TODO: The current implementation ignores the mode flag.

    if (!file) {
        dlerror_set("Invalid NULL file path");
        return NULL;
    }

    char filename[FILE_NAME_MAX] = {0};
    if (!dlmod_getname(file, filename, FILE_NAME_MAX)) {
        dlerror_set("Failed to extract filename from path");
        return NULL;
    }

    void *handle = dlmod_insert(file, filename);
    if (!handle) {
        dlerror_set("Failed to load module");
        return NULL;
    }

    dlerror_set(NULL);  // Clear error on success
    return handle;
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
    if (!handle) {
        dlerror_set("Invalid NULL handle");
        return -1;
    }

    if (!dlmod_validate_handle(handle)) {
        dlerror_set("Invalid module handle");
        return -1;
    }

    int ret = dlmod_remove(handle);
    if (ret != 0) {
        dlerror_set("Failed to close module");
        return -1;
    }

    dlerror_set(NULL);  // Clear error on success
    return 0;
}

/**
 * @brief Dynamic loader compatibility interface - Look up symbol address.
 *
 * @param handle - Module handle from dlopen()
 * @param name - Symbol name to locate
 *
 * @return Memory address of the symbol if found, NULL otherwise.
 */
void *dlsym(void *handle, const char *name)
{
    if (!handle) {
        dlerror_set("Invalid NULL handle");
        return NULL;
    }

    if (!dlmod_validate_handle(handle)) {
        dlerror_set("Invalid module handle");
        return NULL;
    }

    if (!name) {
        dlerror_set("Invalid NULL symbol name");
        return NULL;
    }

    struct dlmod_slist_t *mod = (struct dlmod_slist_t *)handle;

    if (mod->elf && mod->elf->num > 0 && mod->elf->symtab) {
        for (size_t i = 0; i < mod->elf->num; i++) {
            if (mod->elf->symtab[i].name &&
                    strcmp(mod->elf->symtab[i].name, name) == 0) {
                dlerror_set(NULL);  // Clear error on success
                return mod->elf->symtab[i].addr;
            }
        }
    }

    dlerror_set("Symbol not found");
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
    } else {
        ESP_LOGW(TAG, "Unknown list type: %d", type);
    }
}
