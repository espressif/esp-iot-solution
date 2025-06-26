/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mem_pool_t *mem_pool_handle_t;

/**
 * @brief        Create memory pool
 * @note         Memory pool manager a block of memory
 *               It avoids calling system malloc/free frequently
 *               Meanwhile memory can be shared across different modules
 *
 * @param        size: Total memory size for memory pool to manage
 *
 * @return       - NULL: Memory is not enough
 *               - not NULL: Memory pool instance
 */
mem_pool_handle_t mem_pool_create(uint32_t size);

/**
 * @brief        Allocate memory from memory pool
 *
 * @param        pool: Memory pool instance
 * @param        size: Allocated size
 *
 * @return       - not NULL: Allocate ok
 *               - NULL: Allocate memory fail
 */
void *mem_pool_alloc(mem_pool_handle_t pool, uint32_t size);

/**
 * @brief        Check whether request memory size over pool size or not
 *
 * @param        pool: Memory pool instance
 * @param        size: Request memory size
 *
 * @return       - false: Request size is less than pool size
 *               - true: Invalid pool or size over pool size
 */
bool mem_pool_is_over_size(mem_pool_handle_t pool, uint32_t size);

/**
 * @brief        Try allocated memory
 *
 * @param        pool: Memory pool instance
 * @param        size: Allocated size
 *
 * @return       - not NULL: Allocate ok
 *               - NULL: Memory not enough
 */
void *mem_pool_try_alloc(mem_pool_handle_t pool, uint32_t size);

/**
 * @brief        Realloc memory from memory pool
 *
 * @param        pool: Memory pool instance
 * @param        buffer: Old memory pointer
 * @param        size: New size
 *
 * @return       - not NULL: Realloc ok
 *               - NULL: Realloc memory fail
 */
void *mem_pool_realloc(mem_pool_handle_t pool, void *buffer, uint32_t size);

/**
 * @brief        Try realloc memory from memory pool
 *
 * @param        pool: Memory pool instance
 * @param        buffer: Old memory pointer
 * @param        size: New size
 *
 * @return       - not NULL: Realloc ok
 *               - NULL: Realloc memory fail
 */
void *mem_pool_try_realloc(mem_pool_handle_t pool, void *buffer, uint32_t size);

/**
 * @brief        Malloc aligned buffer from memory pool
 *
 * @param        pool: Memory pool instance
 * @param        size: Old memory pointer
 * @param        align_size: New size
 *
 * @return       - not NULL: Allocate ok
 *               - NULL: Allocate memory fail
 */
void *mem_pool_malloc_aligned(mem_pool_handle_t pool, uint32_t size, uint16_t align_size);

/**
 * @brief        Try malloc aligned buffer from memory pool
 *
 * @param        pool: Memory pool instance
 * @param        size: Old memory pointer
 * @param        align_size: New size
 *
 * @return       - not NULL: Allocate ok
 *               - NULL: Allocate memory fail
 */
void *mem_pool_try_malloc_aligned(mem_pool_handle_t pool, uint32_t size, uint16_t align_size);

/**
 * @brief        Free memory from memory pool
 *
 * @param        pool: Memory pool instance
 * @param        buffer: Buffer to free
 */
void mem_pool_free(mem_pool_handle_t pool, void *buffer);

/**
 * @brief        Destroy memory pool
 *
 * @param        pool: Memory pool instance
 */
void mem_pool_destroy(mem_pool_handle_t pool);

#ifdef __cplusplus
}
#endif
