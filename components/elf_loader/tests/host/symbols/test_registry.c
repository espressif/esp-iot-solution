/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../../../src/esp_elf.c"

enum {
    LIBC_TARGET,
    IDF_TARGET,
    CUSTOMER_TARGET,
    RUNTIME_FIRST,
    RUNTIME_SECOND,
    RUNTIME_REUSED,
    SHARED_TARGET,
    TARGET_COUNT,
};

static int targets[TARGET_COUNT];

const struct esp_elfsym g_esp_libc_elfsyms[] = {
    {"collision", &targets[LIBC_TARGET]},
    {"libc_only", &targets[LIBC_TARGET]},
    ESP_ELFSYM_END,
};

const struct esp_elfsym g_esp_espidf_elfsyms[] = {
    {"collision", &targets[IDF_TARGET]},
    {"idf_only", &targets[IDF_TARGET]},
    ESP_ELFSYM_END,
};

const struct esp_elfsym g_customer_elfsyms[] = {
    {"collision", &targets[CUSTOMER_TARGET]},
    {"customer_only", &targets[CUSTOMER_TARGET]},
    {"customer_only", &targets[RUNTIME_FIRST]},
    ESP_ELFSYM_END,
};

static const struct esp_elfsym first_runtime[] = {
    {"collision", &targets[RUNTIME_FIRST]},
    {"runtime_only", &targets[RUNTIME_FIRST]},
    {"shared_only", &targets[RUNTIME_FIRST]},
    ESP_ELFSYM_END,
};

static const struct esp_elfsym second_runtime[] = {
    {"runtime_only", &targets[RUNTIME_SECOND]},
    ESP_ELFSYM_END,
};

static const struct esp_elfsym reused_runtime[] = {
    {"runtime_only", &targets[RUNTIME_REUSED]},
    ESP_ELFSYM_END,
};

#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
void *dlmod_getaddr(const char *name)
{
    return strcmp(name, "shared_only") == 0 ? &targets[SHARED_TARGET] : NULL;
}
#endif

static void check_compiled_tables(void)
{
    const esp_elf_symbol_registry_t *registry = &g_esp_elf_static_symbol_registry;
    assert(registry->version == 1);
    size_t index = 0;
#ifdef CONFIG_ELF_LOADER_LIBC_SYMBOLS
    assert(registry->tables[index++] == g_esp_libc_elfsyms);
    assert(elf_find_sym_default("libc_only") == (uintptr_t)&targets[LIBC_TARGET]);
#else
    assert(elf_find_sym_default("libc_only") == 0);
#endif
#ifdef CONFIG_ELF_LOADER_ESPIDF_SYMBOLS
    assert(registry->tables[index++] == g_esp_espidf_elfsyms);
    assert(elf_find_sym_default("idf_only") == (uintptr_t)&targets[IDF_TARGET]);
#else
    assert(elf_find_sym_default("idf_only") == 0);
#endif
#ifdef CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS
    assert(registry->tables[index++] == g_customer_elfsyms);
    assert(elf_find_sym_default("customer_only") == (uintptr_t)&targets[CUSTOMER_TARGET]);
#else
    assert(elf_find_sym_default("customer_only") == 0);
#endif
    assert(registry->table_count == index);
    assert(registry->tables[index] == NULL);
    assert(elf_find_sym_default(NULL) == 0);
    assert(elf_find_sym_default("missing") == 0);
}

static void check_precedence(void)
{
    assert(esp_elf_register_symbol(first_runtime) == 0);
    assert(esp_elf_register_symbol(second_runtime) == 0);
#ifdef CONFIG_ELF_LOADER_LIBC_SYMBOLS
    assert(elf_find_sym_default("collision") == (uintptr_t)&targets[LIBC_TARGET]);
#elif defined(CONFIG_ELF_LOADER_ESPIDF_SYMBOLS)
    assert(elf_find_sym_default("collision") == (uintptr_t)&targets[IDF_TARGET]);
#elif defined(CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS)
    assert(elf_find_sym_default("collision") == (uintptr_t)&targets[CUSTOMER_TARGET]);
#else
    assert(elf_find_sym_default("collision") == (uintptr_t)&targets[RUNTIME_FIRST]);
#endif
    assert(elf_find_sym_default("runtime_only") == (uintptr_t)&targets[RUNTIME_FIRST]);
    assert(elf_find_sym_default("shared_only") == (uintptr_t)&targets[RUNTIME_FIRST]);
    assert(esp_elf_unregister_symbol(first_runtime) == 0);
    assert(elf_find_sym_default("runtime_only") == (uintptr_t)&targets[RUNTIME_SECOND]);
    assert(esp_elf_register_symbol(reused_runtime) == 0);
    assert(elf_find_sym_default("runtime_only") == (uintptr_t)&targets[RUNTIME_REUSED]);
    assert(esp_elf_unregister_symbol(reused_runtime) == 0);
    assert(esp_elf_unregister_symbol(second_runtime) == 0);
    assert(elf_find_sym_default("runtime_only") == 0);
#if CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT
    assert(elf_find_sym_default("shared_only") == (uintptr_t)&targets[SHARED_TARGET]);
#else
    assert(elf_find_sym_default("shared_only") == 0);
#endif
}

int main(void)
{
    check_compiled_tables();
    check_precedence();
    check_compiled_tables();
    return 0;
}
