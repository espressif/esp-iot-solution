/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief        Allocate extractor configuration for local url file
 *
 * @note         This configuration need free after call `esp_extractor_close`
 *
 * @param        file_url: File URL
 * @param        extract_mask: Extract mask
 * @param        output_size: Memory pool size to hold output frames
 *
 * @return
 *               - NULL: No enough memory
 *               - Others: Extract configuration
 */
esp_extractor_config_t *esp_extractor_alloc_file_config(char *file_url, uint8_t extract_mask, int output_size);

/**
 * @brief        Allocate extractor configuration for buffer
 *
 * @note         This configuration need free after call `esp_extractor_close`
 *
 * @param        buffer: Buffer pointer
 * @param        buffer_size: Buffer size
 * @param        extract_mask: Extract mask
 * @param        output_size: Memory pool size to hold output frames
 *
 * @return
 *               - NULL: No enough memory
 *               - Others: Extract configuration
 */
esp_extractor_config_t *esp_extractor_alloc_buffer_config(uint8_t *buffer, int buffer_size, uint8_t extract_mask,
                                                          int output_size);

/**
 * @brief        Free configuration
 *
 * @param        config: Configuration
 */
void esp_extractor_free_config(esp_extractor_config_t *config);

#ifdef __cplusplus
}
#endif
