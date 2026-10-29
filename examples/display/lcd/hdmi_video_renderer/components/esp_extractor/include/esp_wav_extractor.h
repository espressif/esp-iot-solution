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
 * @brief     Register extractor for WAV container
 *
 * @return
 *        - ESP_EXTR_ERR_OK: Register success
 *        - ESP_EXTR_ERR_ALREADY_EXIST: Already registered
 *        - ESP_EXTR_ERR_NO_MEM: Memory not enough
 */
esp_extr_err_t esp_wav_extractor_register(void);

#ifdef __cplusplus
}
#endif
