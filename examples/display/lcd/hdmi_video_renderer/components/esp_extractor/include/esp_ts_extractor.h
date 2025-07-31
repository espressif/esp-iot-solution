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
 * @brief     Register extractor for MPEG-TS container
 *
 * @return
 *        - ESP_EXTR_ERR_OK: Register success
 *        - ESP_EXTR_ERR_ALREADY_EXIST: Already registered
 *        - ESP_EXTR_ERR_NO_MEM: Memory not enough
 */
esp_extr_err_t esp_ts_extractor_register(void);

/**
 * @brief     Allow audio or video frame across multiple PES
 * @note      Default not allow frame over multiple PES to decrease memory usage
 *            When enable if check frame is not whole in one PES,
 *            Need at least keep 2 frames to combine into one whole frame
 *
 */
void esp_ts_extractor_allow_frame_across_pes(bool enable);

#ifdef __cplusplus
}
#endif
