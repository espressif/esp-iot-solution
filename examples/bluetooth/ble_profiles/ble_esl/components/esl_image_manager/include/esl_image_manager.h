/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t image_id;
    uint8_t image_index;
    uint32_t version;
    const uint8_t *data;
    uint32_t data_len;
} esl_image_desc_t;

esp_err_t esl_image_manager_init(void);
esp_err_t esl_image_manager_get(uint32_t image_id, esl_image_desc_t *out);
esp_err_t esl_image_manager_next_for_tag(uint16_t esl_addr, esl_image_desc_t *out);
esp_err_t esl_image_manager_prev_for_tag(uint16_t esl_addr, esl_image_desc_t *out);
esp_err_t esl_image_manager_mark_transferred(uint16_t esl_addr, uint32_t image_id, uint8_t image_index);
esp_err_t esl_image_manager_store_runtime(uint32_t image_id, uint8_t image_index, uint32_t version,
                                          const uint8_t *data, uint32_t data_len);
uint32_t esl_image_manager_count(void);

#ifdef __cplusplus
}
#endif
