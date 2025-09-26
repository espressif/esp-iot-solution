/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro to export a symbol for ELF loading.
 *
 * @param _sym Symbol name (function or variable)
 *
 * @note The (void*) cast is necessary to convert the symbol address
 *       to the generic pointer type used in esp_elfsym structure.
 *       This is safe for functions and objects with static storage duration.
 */
#define ESP_ELFSYM_EXPORT(_sym)     { #_sym, (void*)&_sym }

/* End of symbol table marker */
#define ESP_ELFSYM_END              { NULL,  NULL }

/** @brief Function symbol description */

struct esp_elfsym {
    const char  *name;      /*!< Function name */
    const void  *sym;       /*!< Function pointer */
};

/**
 * @brief Symbol resolver function type.
 *
 * Resolves a symbol name (e.g. function name) to its address.
 * Custom resolvers allow dynamic symbol lookup beyond the static
 * symbol tables configured by KConfig.
 *
 * @param sym_name Symbol name to resolve (null-terminated string)
 * @return Symbol address if success, or 0 if the symbol is not found.
 *
 * @note The resolver function should be thread-safe if used in multi-threaded contexts.
 * @note The resolver should return 0 for unknown symbols, not a random address.
 *
 * @example Example implementation:
 * ```c
 * uintptr_t my_resolver(const char *sym_name)
 * {
 *     // Check custom symbol table first
 *     if (strcmp(sym_name, "my_function") == 0) {
 *         return (uintptr_t)my_function;
 *     }
 *
 *     // Fall back to default resolver
 *     return elf_find_sym_default(sym_name);
 * }
 *
 * // Usage:
 * elf_set_symbol_resolver(my_resolver);
 * ```
 */
typedef uintptr_t (*symbol_resolver)(const char *sym_name);

/**
 * @brief Find symbol address by name.
 *
 * @param sym_name - Symbol name
 *
 * @return Symbol address if success or 0 if failed.
 */
uintptr_t elf_find_sym(const char *sym_name);

/**
 * @brief Find symbol address by name.
 *
 * @param sym_name - Symbol name
 *
 * @return Symbol address if success or 0 if failed.
 */
uintptr_t elf_find_sym_default(const char *sym_name);

/**
 * @brief Override the internal symbol resolver.
 *
 * This override allows for an arbitrary implementation, enabling:
 * - Dynamic symbol loading
 * - Custom symbol resolution strategies
 * - Symbol interception and hooking
 *
 * @param resolver The resolver function. Must not be NULL.
 *                 To reset to default resolver, use elf_reset_symbol_resolver().
 *
 * @note This function is not thread-safe. Ensure proper synchronization
 *       when calling from multiple threads.
 * @note The resolver function pointer must remain valid during its lifetime.
 *
 * @warning Calling this function affects all subsequent symbol resolutions
 *          globally. Use with caution.
 *
 * @example Example usage:
 * ```c
 * // Set custom resolver
 * elf_set_symbol_resolver(my_custom_resolver);
 *
 * // Load and use ELF file
 * esp_elf_t elf;
 * esp_elf_load(&elf, elf_data);
 *
 * // Reset to default if needed
 * elf_reset_symbol_resolver();
 * ```
 */
void elf_set_symbol_resolver(symbol_resolver resolver);

/**
 * @brief Reset the symbol resolver to the default (static tables from KConfig).
 *
 * Equivalent to elf_set_symbol_resolver(elf_find_sym_default).
 */
void elf_reset_symbol_resolver(void);

#ifdef __cplusplus
}
#endif
