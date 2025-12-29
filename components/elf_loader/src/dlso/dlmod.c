/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_elf.h"
#include "esp_log.h"
#include "esp_dlfcn.h"
#include "private/esp_dlmod.h"
#include "private/elf_platform.h"

static const char *TAG = "DLMOD";

typedef SLIST_HEAD(dlmod_slist_head, dlmod_slist_t) dlmod_slist_head_t;

static dlmod_slist_head_t g_dlmod_slist_head = SLIST_HEAD_INITIALIZER(g_dlmod_slist_head);
static StaticSemaphore_t g_dlmod_mutex_buffer;
static SemaphoreHandle_t g_dlmod_mutex = NULL;
static portMUX_TYPE g_dlmod_mutex_init_lock = portMUX_INITIALIZER_UNLOCKED;

static void dlmod_mutex_init(void)
{
    taskENTER_CRITICAL(&g_dlmod_mutex_init_lock);
    if (g_dlmod_mutex == NULL) {
        g_dlmod_mutex = xSemaphoreCreateMutexStatic(&g_dlmod_mutex_buffer);
        if (g_dlmod_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex for dlmod");
        }
    }
    taskEXIT_CRITICAL(&g_dlmod_mutex_init_lock);
}

bool dlmod_validate_handle(void *handle)
{
    if (!handle) {
        return false;
    }

    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return false;
    }

    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    bool valid = false;
    struct dlmod_slist_t *current;
    SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
        if (current == handle && current->magic == DLMOD_HANDLE_MAGIC) {
            valid = true;
            break;
        }
    }

    xSemaphoreGive(g_dlmod_mutex);
    return valid;
}

/**
 * @brief Extract the base filename from a given path, removing directory and extension.
 *
 * @param path     - Full path to the file
 * @param filename - Buffer to store the extracted filename
 * @param len      - Length of the filename buffer
 *
 * @return Pointer to the filename buffer on success, NULL on invalid parameters or truncation.
 */
char *dlmod_getname(const char *path, char *filename, size_t len)
{
    if (!path || path[0] == '\0' || !filename || len == 0) {
        return NULL;
    }

    const char *filename_start = strrchr(path, '/');
    if (filename_start == NULL) {
        filename_start = path;
    } else {
        filename_start++;
    }

    const char *extension_start = strrchr(filename_start, '.');
    size_t filename_len = (extension_start ? extension_start - filename_start : strlen(filename_start));
    if (filename_len >= len || len == 0) {
        return NULL;
    }

    strncpy(filename, filename_start, filename_len);
    filename[filename_len] = '\0';

    return filename;
}

/**
 * @brief Retrieve a module handle by name from the global module list.
 *
 * @param name - Name of the module to find (without .so extension)
 *
 * @return Pointer to the module handle if found, NULL if not found or invalid parameters.
 */
void *dlmod_gethandle(const char *name)
{
    if (!name || name[0] == '\0') {
        return NULL;
    }

    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return NULL;
    }

    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }

    void *result = NULL;
    if (SLIST_EMPTY(&g_dlmod_slist_head)) {
        ESP_LOGD(TAG, "The list is empty, no module found.");
    } else {
        struct dlmod_slist_t *current;
        SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
            if (strcmp(current->name, name) == 0) {
                result = current;
                break;
            }
        }
    }

    xSemaphoreGive(g_dlmod_mutex);
    return result;
}

/**
 * @brief List all loaded module names in the system.
 */
void dlmod_listname(void)
{
    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (SLIST_EMPTY(&g_dlmod_slist_head)) {
        ESP_LOGI(TAG, "No shared objects loaded");
    } else {
        struct dlmod_slist_t *current;
        ESP_LOGI(TAG, "Shared objects in the list:");
        SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
            ESP_LOGI(TAG, "%s.so", current->name);
        }
    }

    xSemaphoreGive(g_dlmod_mutex);
}

/**
 * @brief List all symbols from all loaded modules with their addresses.
 */
void dlmod_listsymbol(void)
{
    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (SLIST_EMPTY(&g_dlmod_slist_head)) {
        ESP_LOGI(TAG, "Symbols table is empty");
    } else {
        struct dlmod_slist_t *current;
        char *name;
        ESP_LOGI(TAG, "Shared objects symbols table in the list:");
        SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
            if (current->elf && current->elf->symtab && current->elf->num) {
                for (uint16_t i = 0; i < current->elf->num; i++) {
                    name = current->elf->symtab[i].name;
                    ESP_LOGI(TAG, "%s, addr: %p", name ? name : "(null)",
                             current->elf->symtab[i].addr);
                }
            }
        }
    }

    xSemaphoreGive(g_dlmod_mutex);
}

/**
 * @brief Relocate and load an ELF file from the specified path.
 *
 * @param path - Filesystem path to the ELF binary
 *
 * @return Pointer to the relocated ELF structure, NULL on relocation failure.
 */
void *dlmod_relocate(const char *path)
{
    if (!path || path[0] == '\0') {
        return NULL;
    }

    int ret;
    elf_file_t file;
    ret = esp_elf_open(&file, path);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to open file %s", path);
        return NULL;
    }

    ESP_LOGD(TAG, "Open file:%s, len=%d", path, file.size);

    esp_elf_t *elf_dl = (esp_elf_t *)esp_elf_malloc(sizeof(esp_elf_t), false);
    if (!elf_dl) {
        ESP_LOGE(TAG, "Failed to allocate memory for ELF");
        esp_elf_close(&file);
        return NULL;
    }

    ret = esp_elf_init(elf_dl);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        esp_elf_free(elf_dl);
        esp_elf_close(&file);
        return NULL;
    }

    ret = esp_elf_relocate(elf_dl, file.payload);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        esp_elf_deinit(elf_dl);
        esp_elf_free(elf_dl);
        esp_elf_close(&file);
        return NULL;
    }

    esp_elf_close(&file);
    return (void *)elf_dl;
}

/**
 * @brief Insert a new module into the global module list.
 *
 * @param path - Filesystem path to the ELF binary
 * @param name - Module name to register (should match filename base)
 *
 * @return Pointer to the new module entry, NULL on failure (existing entry or relocation error).
 */
void *dlmod_insert(const char *path, const char *name)
{
    if (!path || path[0] == '\0' || !name || name[0] == '\0') {
        return NULL;
    }

    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return NULL;
    }

    // Check if module already exists (with mutex protection)
    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }

    if (!SLIST_EMPTY(&g_dlmod_slist_head)) {
        struct dlmod_slist_t *current;
        SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
            if (strcmp(current->name, name) == 0) {
                ESP_LOGD(TAG, "%s.so has been dynamically loaded", name);
                xSemaphoreGive(g_dlmod_mutex);
                return current;
            }
        }
    }
    xSemaphoreGive(g_dlmod_mutex);

    // Module doesn't exist, create new one (without mutex - these are not list operations)
    struct dlmod_slist_t *new_node;
    new_node = esp_elf_malloc(sizeof(struct dlmod_slist_t), false);
    if (!new_node) {
        ESP_LOGE(TAG, "Failed to allocate memory for new node");
        return NULL;
    }

    new_node->magic = DLMOD_HANDLE_MAGIC;
    strncpy(new_node->name, name, FILE_NAME_MAX - 1);
    new_node->name[FILE_NAME_MAX - 1] = '\0';
    new_node->elf = (esp_elf_t *)dlmod_relocate(path);
    if (!new_node->elf) {
        ESP_LOGE(TAG, "Failed to relocate");
        esp_elf_free(new_node);
        return NULL;
    }

    // Insert into list (with mutex protection)
    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        esp_elf_deinit(new_node->elf);
        esp_elf_free(new_node->elf);
        esp_elf_free(new_node);
        return NULL;
    }

    // Re-check if module was inserted by another thread
    struct dlmod_slist_t *existing;
    SLIST_FOREACH(existing, &g_dlmod_slist_head, next) {
        if (strcmp(existing->name, name) == 0) {
            ESP_LOGD(TAG, "%s.so already loaded by another thread", name);
            xSemaphoreGive(g_dlmod_mutex);
            esp_elf_deinit(new_node->elf);
            esp_elf_free(new_node->elf);
            esp_elf_free(new_node);
            return existing;
        }
    }

    SLIST_INSERT_HEAD(&g_dlmod_slist_head, new_node, next);
    xSemaphoreGive(g_dlmod_mutex);

    return new_node;
}

/**
 * @brief Remove a module from the global module list and free resources.
 *
 * @param handle - Module handle to remove
 *
 * @return 0 on successful removal, -1 if handle not found.
 */
int dlmod_remove(void *handle)
{
    if (!handle) {
        return -1;
    }

    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return -1;
    }

    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    // Find and remove from list (with mutex protection)
    struct dlmod_slist_t *mod = (struct dlmod_slist_t *)handle;
    struct dlmod_slist_t *current;
    struct dlmod_slist_t *prev = NULL;
    int ret = -1;

    current = SLIST_FIRST(&g_dlmod_slist_head);
    while (current != NULL) {
        if (current == handle) {
            if (current->magic != DLMOD_HANDLE_MAGIC) {
                break;
            }
            if (prev == NULL) {
                SLIST_REMOVE_HEAD(&g_dlmod_slist_head, next);
            } else {
                SLIST_REMOVE(&g_dlmod_slist_head, current, dlmod_slist_t, next);
            }

            ESP_LOGI(TAG, "Removed node with name: %s.so", current->name);
            ret = 0;
            break;
        }

        prev = current;
        current = SLIST_NEXT(current, next);
    }

    if (ret != 0) {
        ESP_LOGD(TAG, "Node not found in the list");
    }

    xSemaphoreGive(g_dlmod_mutex);

    // Free resources (without mutex - not a list operation)
    if (ret == 0 && mod && mod->elf) {
        esp_elf_deinit(mod->elf);
        esp_elf_free(mod->elf);
    }
    if (ret == 0) {
        mod->magic = 0;
        esp_elf_free(mod);
    }

    return ret;
}

/**
 * @brief Find a symbol address by name across all loaded modules.
 *
 * @param sym_name - Symbol name to search for
 *
 * @return Memory address of the symbol if found, NULL otherwise.
 */
void *dlmod_getaddr(const char *sym_name)
{
    if (!sym_name || sym_name[0] == '\0') {
        return NULL;
    }

    dlmod_mutex_init();
    if (g_dlmod_mutex == NULL) {
        return NULL;
    }

    if (xSemaphoreTake(g_dlmod_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }

    void *result = NULL;
    struct dlmod_slist_t *current;
    SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
        if (current->elf && current->elf->symtab && current->elf->num) {
            for (uint16_t i = 0; i < current->elf->num; i++) {
                const char *name = current->elf->symtab[i].name;
                if (name && strcmp(name, sym_name) == 0) {
                    ESP_LOGD(TAG, "Found node with sym_name: %s, addr: %p",
                             sym_name, current->elf->symtab[i].addr);
                    result = current->elf->symtab[i].addr;
                    break;
                }
            }
            if (result != NULL) {
                break;
            }
        }
    }

    if (result == NULL) {
        ESP_LOGD(TAG, "Node with elf name '%s' not found", sym_name);
    }

    xSemaphoreGive(g_dlmod_mutex);
    return result;
}
