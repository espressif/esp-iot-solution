/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize GProf.
 *
 * @return ESP_OK if sucess or other if failed.
 */
esp_err_t esp_gprof_init(void);

/**
 * @brief Save the profiling data of your runing code to the specific partition.
 *        Later, you can use esp_gprof_dump to printf them out, or use idf.py
 *        gprof to read it out for analysis.
 *
 * @return ESP_OK if sucess or other if failed.
 */
esp_err_t esp_gprof_save(void);

/**
 * @brief Dump the profiling data of your runing code from the specific partition.
 *
 * @return ESP_OK if sucess or other if failed.
 */
esp_err_t esp_gprof_dump(void);

/**
 * @brief De-initialize GProf.
 *
 * @return ESP_OK if sucess or other if failed.
 */
esp_err_t esp_gprof_deinit(void);

#ifdef __cplusplus
}
#endif
