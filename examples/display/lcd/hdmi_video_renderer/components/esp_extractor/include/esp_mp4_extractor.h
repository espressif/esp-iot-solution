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
 * @brief     Register extractor for MP4 container
 * @return
 *        - ESP_EXTR_ERR_OK: Register success
 *        - ESP_EXTR_ERR_ALREADY_EXIST: Already registered
 *        - ESP_EXTR_ERR_NO_MEM: Memory not enough
 */
esp_extr_err_t esp_mp4_extractor_register(void);

/**
 * @brief     Use dynamic parse for MP4 container
 *
 * @note      For big MP4 file will have large tables which can't be loaded into memory all at once
 *            Dynamic parse is added to allow users to load partial table and parse gradually
 *            When enabled, extra file seek operation is requested and may take longer time for network input
 *            If memory is enough and file size is small, recommend not enable it
 * @param     enable: Enable dynamic parse or not
 */
void esp_mp4_extractor_use_dynamic_parser(bool enable);

/**
 * @brief     Show parse detail log
 *
 * @note      For debug only
 *
 * @param     show: Show parse log
 */
void esp_mp4_extractor_show_parse_log(bool show);

#ifdef __cplusplus
}
#endif
