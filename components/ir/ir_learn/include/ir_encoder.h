/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of IR encoder configuration
 */
typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} ir_encoder_config_t;

/**
 * @brief Create RMT encoder for encoding raw symbols into RMT symbols.
 *
 * @param[in] config Encoder configuration
 * @param[out] ret_encoder Returned encoder handle
 * @return
 *      - ESP_OK                    Create RMT copy encoder successfully
 *      - ESP_ERR_INVALID_ARG       Create RMT copy encoder failed because of invalid argument
 *      - ESP_ERR_NO_MEM            Create RMT copy encoder failed because out of memory
 *      - ESP_FAIL                  Create RMT copy encoder failed because of other error
 */
esp_err_t ir_encoder_new(const ir_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif
