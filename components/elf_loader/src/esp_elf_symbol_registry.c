/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_elf.h"
#include "esp_log.h"

#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
#include "private/esp_dlmod.h"
#endif

static const char *TAG = "ELF_SYMBOL";

#ifdef CONFIG_ELF_LOADER_LIBC_SYMBOLS
extern const struct esp_elfsym g_esp_libc_elfsyms[];
#endif
#ifdef CONFIG_ELF_LOADER_ESPIDF_SYMBOLS
extern const struct esp_elfsym g_esp_espidf_elfsyms[];
#endif
#ifdef CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS
extern const struct esp_elfsym g_customer_elfsyms[];
#endif

static esp_elf_symbol_table_t *const s_static_tables[] = {
#ifdef CONFIG_ELF_LOADER_LIBC_SYMBOLS
    g_esp_libc_elfsyms,
#endif
#ifdef CONFIG_ELF_LOADER_ESPIDF_SYMBOLS
    g_esp_espidf_elfsyms,
#endif
#ifdef CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS
    g_customer_elfsyms,
#endif
    NULL,
};

const esp_elf_symbol_registry_t g_esp_elf_static_symbol_registry __attribute__((used)) = {
    .version = ESP_ELF_SYMBOL_REGISTRY_VERSION,
    .table_count = sizeof(s_static_tables) / sizeof(s_static_tables[0]) - 1,
    .tables = s_static_tables,
};

/**
 * @brief Find symbol address by name.
 *
 * @param sym_name - Symbol name
 *
 * @return Symbol address if success or 0 if failed.
 */
uintptr_t elf_find_sym_default(const char *sym_name)
{
    if (!sym_name) {
        ESP_LOGE(TAG, "Invalid parameter: sym_name is NULL");
        return 0;
    }

    const esp_elf_symbol_registry_t *registry = &g_esp_elf_static_symbol_registry;
    for (uint32_t i = 0; i < registry->table_count; ++i) {
        for (esp_elf_symbol_table_t *symbol = registry->tables[i]; symbol->name; ++symbol) {
            if (!strcmp(symbol->name, sym_name)) {
                return (uintptr_t)symbol->sym;
            }
        }
    }

    uintptr_t sym_addr = esp_elf_find_symbol(sym_name);
    if (sym_addr) {
        return sym_addr;
    }

#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    return (uintptr_t)dlmod_getaddr(sym_name);
#else
    return 0;
#endif
}
