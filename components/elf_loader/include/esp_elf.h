/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "private/elf_types.h"
#include "private/elf_symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Symbol table type
 *
 * A symbol table is an array of esp_elfsym structures terminated with ESP_ELFSYM_END.
 */
typedef const struct esp_elfsym esp_elf_symbol_table_t;

/**
 * @brief Map symbol's address of ELF to physic space.
 *
 * @param elf - ELF object pointer
 * @param sym - ELF symbol address
 *
 * @return Mapped physic address.
 */
uintptr_t esp_elf_map_sym(esp_elf_t *elf, uintptr_t sym);

/**
 * @brief Initialize ELF object.
 *
 * @param elf - ELF object pointer
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_init(esp_elf_t *elf);

/**
 * @brief Decode and relocate ELF data.
 *
 * @param elf - ELF object pointer
 * @param pbuf - ELF data buffer
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_relocate(esp_elf_t *elf, const uint8_t *pbuf);

/**
 * @brief Request running relocated ELF function.
 *
 * @param elf  - ELF object pointer
 * @param opt  - Request options
 * @param argc - Arguments number
 * @param argv - Arguments value array
 *
 * @return ESP_OK if success or other if failed.
 */
int esp_elf_request(esp_elf_t *elf, int opt, int argc, char *argv[]);

/**
 * @brief Deinitialize ELF object.
 *
 * @param elf - ELF object pointer
 *
 * @return None
 */
void esp_elf_deinit(esp_elf_t *elf);

/**
 * @brief Print header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_ehdr(const uint8_t *pbuf);

/**
 * @brief Print program header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_phdr(const uint8_t *pbuf);

/**
 * @brief Print section header description information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_shdr(const uint8_t *pbuf);

/**
 * @brief Print section information of ELF.
 *
 * @param pbuf - ELF data buffer
 *
 * @return None
 */
void esp_elf_print_sec(esp_elf_t *elf);

/**
 * @brief Register symbol table to global symbol tables array.
 *
 * @param symbol_table - Pointer to symbol table structure (array of esp_elfsym terminated by ESP_ELFSYM_END)
 *
 * @return 0 if success, -EINVAL if symbol_table is NULL, -EEXIST if already registered, -ENOMEM if no space.
 *
 * @note This function is not thread-safe. External synchronization must be used if calling
 *       this function concurrently from multiple threads.
 */
int esp_elf_register_symbol(esp_elf_symbol_table_t *symbol_table);

/**
 * @brief Unregister symbol table from global symbol tables array.
 *
 * @param symbol_table - Pointer to symbol table structure to remove
 *
 * @return 0 if success, -EINVAL if symbol_table is NULL or symbol table not found.
 *
 * @note This function is not thread-safe. External synchronization must be used if calling
 *       this function concurrently from multiple threads.
 */
int esp_elf_unregister_symbol(esp_elf_symbol_table_t *symbol_table);

/**
 * @brief Find symbol address by symbol name in registered tables.
 *
 * @param sym_name - Symbol name string to search
 *
 * @return Symbol address if found, 0 if not found.
 * @note Search order is reverse registration order (latest registered first).
 */
uintptr_t esp_elf_find_symbol(const char *sym_name);

#ifdef __cplusplus
}
#endif
