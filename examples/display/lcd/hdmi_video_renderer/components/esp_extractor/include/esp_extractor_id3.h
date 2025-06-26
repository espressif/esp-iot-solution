/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_extractor.h"

/**
 * @brief ID3 reader callback
 */
typedef int (*id3_read_cb)(uint8_t *data, int size, void *read_ctx);

/**
 * @brief ID3 parse callback
 */
typedef int (*id3_parse_cb)(id3_read_cb reader, void *read_ctx, void *parse_ctx);

/**
 * @brief ID3 parse configuration
 * @note  Parser need handle parse function in parse callback
 *        Read API and read context is provided in parse callback
 */
typedef struct {
    id3_parse_cb parse_cb;  /*!< ID3 parse callback */
    void        *parse_ctx; /*!< ID3 parse context */
} extractor_id3_parse_cfg_t;

/**
 * @brief        Parse ID3 data and get ID3 information
 *
 * @param        extractor: Extractor Handle
 * @param        id3_info: ID3 information
 *
 * @return       - ESP_EXTR_ERR_OK: On success
 *               - ESP_EXTR_ERR_INV_ARG: Invalid input arguments
 *               - Others: Fail to parse ID3 information
 */
esp_extr_err_t esp_extractor_parse_id3(esp_extractor_handle_t extractor, extractor_id3_parse_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
