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
 * @brief     OGG vorbis specified information
 * @note      Comment header is skipped for not used by decoder
 */
typedef struct {
    uint8_t *info_header;  /*!< Information header data */
    uint32_t info_size;    /*!< Information header size */
    uint8_t *setup_header; /*!< Setup header data */
    uint32_t setup_size;   /*!< Setup header size */
} ogg_vorbis_spec_info_t;

/**
 * @brief     Register extractor for OGG container
 *
 * @return
 *        - ESP_EXTR_ERR_OK: Register success
 *        - ESP_EXTR_ERR_ALREADY_EXIST: Already registered
 *        - ESP_EXTR_ERR_NO_MEM: Memory not enough
 */
esp_extr_err_t esp_ogg_extractor_register(void);

#ifdef __cplusplus
}
#endif
