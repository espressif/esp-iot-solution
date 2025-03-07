/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>

#include "esp_elf.h"
#include "esp_log.h"
#include "esp_dlfcn.h"
#include "private/esp_dlmod.h"
#include "private/elf_platform.h"

static const char *TAG = "DLMOD";

typedef SLIST_HEAD(dlmod_slist_head, dlmod_slist_t) dlmod_slist_head_t;

static dlmod_slist_head_t g_dlmod_slist_head = SLIST_HEAD_INITIALIZER(g_dlmod_slist_head);

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
    if (filename_len >= len) {
        filename_len = len - 1;
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

    if (SLIST_EMPTY(&g_dlmod_slist_head)) {
        ESP_LOGD(TAG, "The list is empty, no module found.");
        return NULL;
    }

    struct dlmod_slist_t *current;
    SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
    }

    return NULL;
}

/**
 * @brief List all loaded module names in the system.
 */
void dlmod_listname(void)
{
    if (SLIST_EMPTY(&g_dlmod_slist_head)) {
        ESP_LOGI(TAG, "Shared objects is empty");
        return;
    }

    struct dlmod_slist_t *current;
    ESP_LOGI(TAG, "Shared objects in the list:");
    SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
        ESP_LOGI(TAG, "%s.so", current->name);
    }
}

/**
 * @brief List all symbols from all loaded modules with their addresses.
 */
void dlmod_listsymbol(void)
{
    if (SLIST_EMPTY(&g_dlmod_slist_head)) {
        ESP_LOGI(TAG, "Symbols table is empty");
        return;
    }

    struct dlmod_slist_t *current;
    char *name;
    ESP_LOGI(TAG, "Shared objects symbols table in the list:");
    SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
        if (current && current->elf && current->elf->num) {
            for (int i = 0; i < current->elf->num; i++) {
                name = current->elf->symtab[i].name;
                ESP_LOGI(TAG, "%s, addr: %p", name ? name : "(null)",
                         current->elf->symtab[i].addr);
            }
        }
    }
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
    void *handle;
    assert(path != NULL && name != NULL);

    handle = dlmod_gethandle(name);
    if (handle) {
        ESP_LOGD(TAG, "%s.so has been dynamically loaded", name);
        return handle;
    }

    struct dlmod_slist_t *new_node;
    new_node = esp_elf_malloc(sizeof(struct dlmod_slist_t), false);
    if (!new_node) {
        ESP_LOGE(TAG, "Failed to allocate memory for new node");
        return NULL;
    }

    strncpy(new_node->name, name, FILE_NAME_MAX - 1);
    new_node->name[FILE_NAME_MAX - 1] = '\0';
    new_node->elf = (esp_elf_t *)dlmod_relocate(path);
    if (!new_node->elf) {
        ESP_LOGE(TAG, "Failed to relocate");
        esp_elf_free(new_node);
        return NULL;
    }

    SLIST_INSERT_HEAD(&g_dlmod_slist_head, new_node, next);

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

    struct dlmod_slist_t *mod = (struct dlmod_slist_t *)handle;
    if (mod && mod->elf) {
        esp_elf_deinit(mod->elf);
        esp_elf_free(mod->elf);
    }

    struct dlmod_slist_t *current;
    struct dlmod_slist_t *prev = NULL;
    current = SLIST_FIRST(&g_dlmod_slist_head);
    while (current != NULL) {
        if (current == handle) {
            if (prev == NULL) {
                SLIST_REMOVE_HEAD(&g_dlmod_slist_head, next);
            } else {
                SLIST_REMOVE(&g_dlmod_slist_head, current, dlmod_slist_t, next);
            }

            ESP_LOGI(TAG, "Removed node with name: %s.so", current->name);
            esp_elf_free(current);
            return 0;
        }

        prev = current;
        current = SLIST_NEXT(current, next);
    }

    ESP_LOGD(TAG, "Node not found in the list");
    return -1;
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

    struct dlmod_slist_t *current;
    SLIST_FOREACH(current, &g_dlmod_slist_head, next) {
        if (current && current->elf && current->elf->num) {
            for (int i = 0; i < current->elf->num; i++) {
                if (strcmp(current->elf->symtab[i].name, sym_name) == 0) {
                    ESP_LOGD(TAG, "Found node with sym_name: %s, addr: %p",
                             sym_name, current->elf->symtab[i].addr);
                    return current->elf->symtab[i].addr;
                }
            }
        }
    }

    ESP_LOGD(TAG, "Node with elf name '%s' not found", sym_name);
    return NULL;
}
